#include "NodeOtaRelay.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <MD5Builder.h>
#include <FirmwareVersion.h>

namespace {
constexpr const char* IMAGE_PATH = "/node-ota.bin";
constexpr const char* META_PATH = "/node-ota.json";

constexpr uint32_t NODE_ACTIVE_MS = 90000;            // heartbeats arrive every 30 s
constexpr uint32_t SCHEDULE_INTERVAL_MS = 1000;
constexpr uint32_t OFFER_INTERVAL_MS = 500;
constexpr uint16_t OFFER_ATTEMPTS = 10;
constexpr uint32_t ACK_TIMEOUT_MS = 250;
constexpr uint16_t CHUNK_ATTEMPTS = 40;
constexpr uint32_t FINISH_TIMEOUT_MS = 20000;
constexpr uint32_t SESSION_LIMIT_MS = 20UL * 60UL * 1000UL;
constexpr uint32_t DOWNLOAD_RETRY_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t AFTER_INSTALL_WAIT_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t BACKOFF_BASE_MS = 2UL * 60UL * 1000UL;
constexpr uint8_t BACKOFF_MAX_SHIFT = 6;               // caps at ~2 h
constexpr uint32_t DOWNLOAD_IDLE_TIMEOUT_MS = 15000;

bool due(uint32_t now, uint32_t at) {
    return static_cast<int32_t>(now - at) >= 0;
}

void copyText(char* destination, size_t capacity, const char* source) {
    if (capacity == 0) return;
    strncpy(destination, source == nullptr ? "" : source, capacity - 1);
    destination[capacity - 1] = '\0';
}
} // namespace

void NodeOtaRelay::begin(SendFunction sendFunction) {
    send = sendFunction;
}

void NodeOtaRelay::clearReleases() {
    for (ReleaseSlot& slot : releases) slot = ReleaseSlot{};
}

void NodeOtaRelay::addRelease(const ShOta::Release& release) {
    ReleaseSlot* slot = findRelease(release.target);
    if (slot == nullptr) {
        for (ReleaseSlot& candidate : releases) {
            if (candidate.release.target[0] == '\0') {
                slot = &candidate;
                break;
            }
        }
    }
    if (slot == nullptr) {
        Serial.printf("[NodeOTA] No room to track release for %s\n", release.target);
        return;
    }
    slot->release = release;
}

NodeOtaRelay::ReleaseSlot* NodeOtaRelay::findRelease(const char* target) {
    if (target == nullptr || target[0] == '\0') return nullptr;
    for (ReleaseSlot& slot : releases) {
        if (strcmp(slot.release.target, target) == 0) return &slot;
    }
    return nullptr;
}

NodeOtaRelay::NodeRecord* NodeOtaRelay::findNode(const uint8_t* mac) {
    for (NodeRecord& record : nodes) {
        if (record.used && memcmp(record.mac, mac, 6) == 0) return &record;
    }
    return nullptr;
}

void NodeOtaRelay::noteNode(const uint8_t* mac, const char* deviceId,
                            const char* target, const char* reportedVersion) {
    if (mac == nullptr || target == nullptr || target[0] == '\0') return;

    NodeRecord* record = findNode(mac);
    if (record == nullptr) {
        // Take a free slot, or evict the node heard from least recently.
        NodeRecord* oldest = nullptr;
        for (NodeRecord& candidate : nodes) {
            if (&candidate == node) continue;  // never evict the node being updated
            if (!candidate.used) {
                oldest = &candidate;
                break;
            }
            if (oldest == nullptr || candidate.lastSeenAt < oldest->lastSeenAt) oldest = &candidate;
        }
        if (oldest == nullptr) return;
        *oldest = NodeRecord{};
        record = oldest;
        record->used = true;
        memcpy(record->mac, mac, 6);
    }

    if (strcmp(record->version, reportedVersion == nullptr ? "" : reportedVersion) != 0) {
        // A new version (e.g. after an update) gets a clean slate.
        record->failures = 0;
        record->waitRetry = false;
    }
    copyText(record->deviceId, sizeof(record->deviceId), deviceId);
    copyText(record->target, sizeof(record->target), target);
    copyText(record->version, sizeof(record->version), reportedVersion);
    record->lastSeenAt = millis();
}

bool NodeOtaRelay::nodeNeedsUpdate(const NodeRecord& record, uint32_t now) {
    if (!record.used || record.target[0] == '\0') return false;
    if (now - record.lastSeenAt > NODE_ACTIVE_MS) return false;
    if (record.waitRetry && !due(now, record.retryAt)) return false;

    ReleaseSlot* slot = findRelease(record.target);
    if (slot == nullptr || !slot->release.valid()) return false;
    if (strcmp(record.skipVersion, slot->release.version) == 0) return false;
    return ShFirmware::isNewer(slot->release.version, record.version);
}

void NodeOtaRelay::loop(bool networkReady) {
    const uint32_t now = millis();

    switch (phase) {
        case Phase::Idle:
            if (now - lastScheduleAt >= SCHEDULE_INTERVAL_MS) {
                lastScheduleAt = now;
                scheduleNext(networkReady, now);
            }
            return;

        case Phase::Offering:
            if (now - lastSendAt >= OFFER_INTERVAL_MS) {
                if (attempts >= OFFER_ATTEMPTS) {
                    fail(OtaStatus::Timeout, false);
                } else {
                    sendOffer();
                }
            }
            break;

        case Phase::Sending:
            if (now - lastSendAt >= ACK_TIMEOUT_MS) {
                if (++attempts > CHUNK_ATTEMPTS) {
                    fail(OtaStatus::Timeout, false);
                } else {
                    sendChunk();
                }
            }
            break;

        case Phase::Finishing:
            if (now - lastSendAt >= FINISH_TIMEOUT_MS) {
                // The Complete broadcast may simply have been lost; the node's
                // next heartbeat shows whether it is on the new version.
                Serial.printf("[NodeOTA] %s: no completion report; will check its version later\n",
                              node->deviceId);
                finish();
            }
            break;
    }

    if (phase != Phase::Idle && now - startedAt > SESSION_LIMIT_MS) {
        fail(OtaStatus::Timeout, false);
    }
}

void NodeOtaRelay::scheduleNext(bool networkReady, uint32_t now) {
    bool anyNeedsUpdate = false;
    for (const NodeRecord& record : nodes) {
        if (nodeNeedsUpdate(record, now)) {
            anyNeedsUpdate = true;
            break;
        }
    }
    // Mount (and on first use, format) LittleFS only once a node needs it;
    // this also loads an image cached before the last reboot.
    if (!anyNeedsUpdate || !ensureFilesystem()) return;

    NodeRecord* candidate = nullptr;
    ReleaseSlot* candidateRelease = nullptr;

    // Prefer nodes whose image is already cached, so a mixed fleet does not
    // re-download images back and forth.
    for (uint8_t pass = 0; pass < 2 && candidate == nullptr; ++pass) {
        for (NodeRecord& record : nodes) {
            if (!nodeNeedsUpdate(record, now)) continue;
            ReleaseSlot* slot = findRelease(record.target);
            const bool cached = cacheMatches(slot->release);
            if (pass == 0 && !cached) continue;
            if (!cached && (!networkReady || (slot->waitRetry && !due(now, slot->retryAt)))) continue;
            candidate = &record;
            candidateRelease = slot;
            break;
        }
    }
    if (candidate == nullptr) return;

    if (!cacheMatches(candidateRelease->release)) {
        Serial.printf("[NodeOTA] Downloading %s %s for %s\n", candidateRelease->release.target,
                      candidateRelease->release.version, candidate->deviceId);
        if (!downloadImage(candidateRelease->release)) {
            candidateRelease->waitRetry = true;
            candidateRelease->retryAt = millis() + DOWNLOAD_RETRY_MS;
            return;
        }
        candidateRelease->waitRetry = false;
    }

    start(*candidate, candidateRelease->release);
}

void NodeOtaRelay::start(NodeRecord& record, const ShOta::Release& release) {
    image = LittleFS.open(IMAGE_PATH, "r");
    if (!image || image.size() != release.size) {
        Serial.println("[NodeOTA] Cached image unreadable; discarding it");
        if (image) image.close();
        cache.valid = false;
        LittleFS.remove(IMAGE_PATH);
        LittleFS.remove(META_PATH);
        return;
    }

    node = &record;
    session = esp_random() | 1U;
    size = release.size;
    offset = 0;
    attempts = 0;
    lastDecile = 0;
    startedAt = millis();
    copyText(version, sizeof(version), release.version);
    phase = Phase::Offering;

    Serial.printf("[NodeOTA] Offering %s %s (%u bytes) to %s (running %s)\n", release.target,
                  release.version, static_cast<unsigned>(size), record.deviceId,
                  record.version[0] ? record.version : "unknown");
    sendOffer();
}

void NodeOtaRelay::onPacket(const uint8_t* mac, const OtaPacket& packet) {
    if (phase == Phase::Idle || node == nullptr) return;
    if (memcmp(mac, node->mac, 6) != 0 || packet.header.session != session) return;

    const OtaMessage type = static_cast<OtaMessage>(packet.header.type);
    const OtaStatus status = static_cast<OtaStatus>(packet.header.status);

    switch (type) {
        case OtaMessage::Accept:
            if (phase != Phase::Offering) return;
            if (packet.header.offset > size) {
                fail(OtaStatus::InvalidOffer, false);
                return;
            }
            offset = packet.header.offset;
            attempts = 0;
            phase = Phase::Sending;
            Serial.printf("[NodeOTA] %s accepted; sending\n", node->deviceId);
            sendChunk();
            break;

        case OtaMessage::Reject: {
            // Rejections that will not change for this version are remembered.
            const bool permanent = status == OtaStatus::WrongTarget || status == OtaStatus::NotNewer ||
                                   status == OtaStatus::BadVersion || status == OtaStatus::BeginFailed ||
                                   status == OtaStatus::InvalidOffer;
            Serial.printf("[NodeOTA] %s rejected %s: %s\n", node->deviceId, version,
                          ShOtaProtocol::statusName(status));
            fail(status, permanent);
            break;
        }

        case OtaMessage::Ack: {
            if (phase != Phase::Sending) return;
            const uint32_t next = packet.header.offset;
            // Only forward progress matters; duplicate/late acks are ignored
            // and losses are covered by the resend timer.
            if (next <= offset) return;
            if (next > size) {
                fail(OtaStatus::InvalidOffer, false);
                return;
            }
            offset = next;
            attempts = 0;

            const uint8_t decile = static_cast<uint8_t>(static_cast<uint64_t>(offset) * 10 / size);
            if (decile != lastDecile) {
                lastDecile = decile;
                Serial.printf("[NodeOTA] %s: %u%%\n", node->deviceId, decile * 10);
            }

            if (offset == size) {
                phase = Phase::Finishing;
                lastSendAt = millis();
            } else {
                sendChunk();
            }
            break;
        }

        case OtaMessage::Complete:
            if (status == OtaStatus::Ok) {
                Serial.printf("[NodeOTA] %s installed %s and is rebooting\n", node->deviceId, version);
                finish();
            } else {
                Serial.printf("[NodeOTA] %s failed to install %s: %s\n", node->deviceId, version,
                              ShOtaProtocol::statusName(status));
                fail(status, false);
            }
            break;

        case OtaMessage::Abort:
            Serial.printf("[NodeOTA] %s aborted: %s\n", node->deviceId, ShOtaProtocol::statusName(status));
            fail(status, false);
            break;

        default:
            break;
    }
}

void NodeOtaRelay::finish() {
    if (node != nullptr) {
        node->failures = 0;
        node->waitRetry = true;
        node->retryAt = millis() + AFTER_INSTALL_WAIT_MS;
    }
    if (image) image.close();
    node = nullptr;
    phase = Phase::Idle;
}

void NodeOtaRelay::fail(OtaStatus status, bool skipVersion) {
    if (node != nullptr) {
        sendControl(OtaMessage::Abort, status);
        if (skipVersion) copyText(node->skipVersion, sizeof(node->skipVersion), version);
        if (node->failures < 255) ++node->failures;
        const uint8_t shift = node->failures - 1 < BACKOFF_MAX_SHIFT ? node->failures - 1 : BACKOFF_MAX_SHIFT;
        node->waitRetry = true;
        node->retryAt = millis() + (BACKOFF_BASE_MS << shift);
        Serial.printf("[NodeOTA] Update of %s stopped (%s)%s\n", node->deviceId,
                      ShOtaProtocol::statusName(status),
                      skipVersion ? "; this version will not be offered again" : "; will retry later");
    }
    if (image) image.close();
    node = nullptr;
    phase = Phase::Idle;
}

void NodeOtaRelay::abortTransfer() {
    if (phase == Phase::Idle) return;
    sendControl(OtaMessage::Abort, OtaStatus::Aborted);
    if (image) image.close();
    node = nullptr;
    phase = Phase::Idle;
}

void NodeOtaRelay::sendOffer() {
    OtaPacket packet;
    ShOtaProtocol::initPacket(packet, OtaMessage::Offer, session);
    OtaOffer offer{};
    offer.size = size;
    copyText(offer.target, sizeof(offer.target), cache.target);
    copyText(offer.version, sizeof(offer.version), cache.version);
    copyText(offer.md5, sizeof(offer.md5), cache.md5);
    memcpy(packet.data, &offer, sizeof(offer));
    packet.header.length = sizeof(offer);

    transmit(packet);
    lastSendAt = millis();
    ++attempts;
}

void NodeOtaRelay::sendChunk() {
    OtaPacket packet;
    ShOtaProtocol::initPacket(packet, OtaMessage::Data, session, offset);
    const uint32_t remaining = size - offset;
    const size_t length = remaining < SH_OTA_CHUNK_SIZE ? remaining : SH_OTA_CHUNK_SIZE;

    if (!image.seek(offset) || image.read(packet.data, length) != length) {
        Serial.println("[NodeOTA] Cached image read failed");
        fail(OtaStatus::WriteFailed, false);
        cache.valid = false;
        LittleFS.remove(IMAGE_PATH);
        LittleFS.remove(META_PATH);
        return;
    }
    packet.header.length = static_cast<uint16_t>(length);
    transmit(packet);
    lastSendAt = millis();
}

void NodeOtaRelay::sendControl(OtaMessage type, OtaStatus status) {
    if (node == nullptr) return;
    OtaPacket packet;
    ShOtaProtocol::initPacket(packet, type, session, offset, status);
    transmit(packet);
}

bool NodeOtaRelay::transmit(const OtaPacket& packet) {
    if (send == nullptr || node == nullptr) return false;
    return send(node->mac, reinterpret_cast<const uint8_t*>(&packet), ShOtaProtocol::packetSize(packet));
}

// --- Image cache (LittleFS) -------------------------------------------------

bool NodeOtaRelay::ensureFilesystem() {
    if (filesystemReady) return true;
    if (filesystemFailed) return false;
    // Formats the data partition on first use (takes a few seconds once).
    if (!LittleFS.begin(true)) {
        Serial.println("[NodeOTA] LittleFS unavailable; node updates disabled");
        filesystemFailed = true;
        return false;
    }
    filesystemReady = true;
    loadCacheMeta();
    return true;
}

void NodeOtaRelay::loadCacheMeta() {
    cache = CachedImage{};
    File meta = LittleFS.open(META_PATH, "r");
    if (!meta) return;
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, meta);
    meta.close();
    if (error) return;

    copyText(cache.target, sizeof(cache.target), doc["target"] | "");
    copyText(cache.version, sizeof(cache.version), doc["version"] | "");
    copyText(cache.md5, sizeof(cache.md5), doc["md5"] | "");
    cache.size = doc["size"] | 0;

    File file = LittleFS.open(IMAGE_PATH, "r");
    cache.valid = file && file.size() == cache.size && cache.size > 0;
    if (file) file.close();
}

bool NodeOtaRelay::cacheMatches(const ShOta::Release& release) const {
    return filesystemReady && cache.valid && cache.size == release.size &&
           strcmp(cache.target, release.target) == 0 &&
           strcmp(cache.version, release.version) == 0 &&
           strcmp(cache.md5, release.md5) == 0;
}

bool NodeOtaRelay::downloadImage(const ShOta::Release& release) {
    if (!ensureFilesystem()) return false;
    if (cacheMatches(release)) return true;

    cache.valid = false;
    LittleFS.remove(IMAGE_PATH);
    LittleFS.remove(META_PATH);

    const size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
    if (release.size + 16384 > freeBytes) {
        Serial.printf("[NodeOTA] Image (%u bytes) does not fit in LittleFS (%u free)\n",
                      static_cast<unsigned>(release.size), static_cast<unsigned>(freeBytes));
        return false;
    }

    ShOta::SecureClient tls;
    HTTPClient http;
    int status = 0;
    if (!ShOta::openUrl(http, tls.client, release.url, status)) {
        Serial.printf("[NodeOTA] Download failed: HTTP %d\n", status);
        http.end();
        return false;
    }
    if (http.getSize() != static_cast<int>(release.size)) {
        Serial.printf("[NodeOTA] Size mismatch: server %d, manifest %u\n", http.getSize(),
                      static_cast<unsigned>(release.size));
        http.end();
        return false;
    }

    File file = LittleFS.open(IMAGE_PATH, "w");
    if (!file) {
        Serial.println("[NodeOTA] Cannot create image file");
        http.end();
        return false;
    }

    MD5Builder md5;
    md5.begin();
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    uint32_t received = 0;
    uint32_t lastDataAt = millis();
    bool writeFailed = false;

    while (received < release.size) {
        const size_t available = stream->available();
        if (available > 0) {
            size_t wanted = available < sizeof(buffer) ? available : sizeof(buffer);
            if (wanted > release.size - received) wanted = release.size - received;
            const size_t read = stream->readBytes(buffer, wanted);
            if (read == 0) continue;
            if (file.write(buffer, read) != read) {
                writeFailed = true;
                break;
            }
            md5.add(buffer, read);
            received += read;
            lastDataAt = millis();
        } else if (!stream->connected() || millis() - lastDataAt > DOWNLOAD_IDLE_TIMEOUT_MS) {
            break;
        } else {
            delay(1);
        }
    }
    file.close();
    http.end();
    md5.calculate();

    if (writeFailed || received != release.size || md5.toString() != release.md5) {
        Serial.printf("[NodeOTA] Download incomplete or corrupt (%u/%u bytes%s)\n",
                      static_cast<unsigned>(received), static_cast<unsigned>(release.size),
                      writeFailed ? ", flash write failed" : "");
        LittleFS.remove(IMAGE_PATH);
        return false;
    }

    JsonDocument doc;
    doc["target"] = release.target;
    doc["version"] = release.version;
    doc["md5"] = release.md5;
    doc["size"] = release.size;
    File meta = LittleFS.open(META_PATH, "w");
    if (!meta || serializeJson(doc, meta) == 0) {
        if (meta) meta.close();
        LittleFS.remove(IMAGE_PATH);
        return false;
    }
    meta.close();

    copyText(cache.target, sizeof(cache.target), release.target);
    copyText(cache.version, sizeof(cache.version), release.version);
    copyText(cache.md5, sizeof(cache.md5), release.md5);
    cache.size = release.size;
    cache.valid = true;
    Serial.printf("[NodeOTA] Cached %s %s (%u bytes)\n", release.target, release.version,
                  static_cast<unsigned>(release.size));
    return true;
}
