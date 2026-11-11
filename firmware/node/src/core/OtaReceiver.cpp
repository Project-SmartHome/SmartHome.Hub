#include "OtaReceiver.h"

#include <string.h>
#include <FirmwareVersion.h>
#include <OtaConfig.h>
#include <OtaRollback.h>

#if defined(ESP32)
    #include <Update.h>
#elif defined(ESP8266)
    #include <Updater.h>
#endif

namespace {
// The gateway resends within 250 ms; this long a silence means it is gone.
constexpr uint32_t TRANSFER_IDLE_TIMEOUT_MS = 30000;
// Complete is a broadcast and may be lost; send it a few times.
constexpr uint8_t COMPLETE_REPEATS = 3;

void abortUpdater() {
#if defined(ESP32)
    Update.abort();
#else
    Update.end(false);  // discards an unfinished image
#endif
}
} // namespace

OtaReceiver* OtaReceiver::instance = nullptr;

OtaReceiver::OtaReceiver(INodeTransport& transport, RegistrationManager& registration)
    : transport(transport), registration(registration) {}

void OtaReceiver::begin() {
    instance = this;
    transport.setRawReceiveCallback(rawCallback);
}

void OtaReceiver::rawCallback(const MacAddress& sender, const uint8_t* data, size_t length) {
    if (instance == nullptr || instance->incomingReady) return;
    if (!ShOtaProtocol::parsePacket(data, length, instance->incoming)) return;
    instance->incomingFrom = sender;
    instance->incomingReady = true;
}

void OtaReceiver::update() {
    if (incomingReady) {
        const OtaPacket packet = incoming;
        const MacAddress sender = incomingFrom;
        incomingReady = false;
        handlePacket(sender, packet);
    }

    if (running && millis() - lastPacketAt > TRANSFER_IDLE_TIMEOUT_MS) {
        Serial.println("[OTA] Gateway went silent; abandoning update");
        stop(OtaStatus::Timeout, false);
    }
}

void OtaReceiver::handlePacket(const MacAddress& sender, const OtaPacket& packet) {
    if (!registration.hasGateway() ||
        memcmp(sender.bytes, registration.gatewayAddress().bytes, sizeof(sender.bytes)) != 0) {
        return;  // only our own gateway may update us
    }
    registration.notifyGatewayActivity();

    switch (static_cast<OtaMessage>(packet.header.type)) {
        case OtaMessage::Offer:
            handleOffer(packet);
            break;
        case OtaMessage::Data:
            if (running && packet.header.session == session) handleData(packet);
            break;
        case OtaMessage::Abort:
            if (running && packet.header.session == session) {
                Serial.printf("[OTA] Gateway cancelled the update (%s)\n",
                              ShOtaProtocol::statusName(static_cast<OtaStatus>(packet.header.status)));
                stop(OtaStatus::Aborted, false);
            }
            break;
        default:
            break;  // node-originated types
    }
}

void OtaReceiver::handleOffer(const OtaPacket& packet) {
    const uint32_t offered = packet.header.session;

    if (running && offered == session) {
        // Our Accept was lost; repeat it with where we are.
        reply(OtaMessage::Accept, session, OtaStatus::Ok, received);
        return;
    }
    if (running) {
        // The gateway restarted the transfer (e.g. it rebooted).
        stop(OtaStatus::Aborted, false);
    }

#if defined(SH_OTA_DISABLED)
    reply(OtaMessage::Reject, offered, OtaStatus::Busy, 0);
    return;
#endif

    if (packet.header.length < sizeof(OtaOffer)) {
        reply(OtaMessage::Reject, offered, OtaStatus::InvalidOffer, 0);
        return;
    }
    OtaOffer offer{};
    memcpy(&offer, packet.data, sizeof(offer));
    offer.target[sizeof(offer.target) - 1] = '\0';
    offer.version[sizeof(offer.version) - 1] = '\0';
    offer.md5[sizeof(offer.md5) - 1] = '\0';

    OtaStatus verdict = OtaStatus::Ok;
    if (strcmp(offer.target, SH_FIRMWARE_TARGET) != 0) {
        verdict = OtaStatus::WrongTarget;
    } else if (!ShFirmware::isNewer(offer.version, SH_FIRMWARE_VERSION)) {
        verdict = OtaStatus::NotNewer;
    } else if (ShOtaRollback::isBad(offer.version)) {
        verdict = OtaStatus::BadVersion;
    } else if (offer.size == 0 || !ShOtaProtocol::isMd5Hex(offer.md5)) {
        verdict = OtaStatus::InvalidOffer;
    } else if (!Update.begin(offer.size)) {
        verdict = OtaStatus::BeginFailed;
    }

    if (verdict != OtaStatus::Ok) {
        Serial.printf("[OTA] Declined %s %s: %s\n", offer.target, offer.version,
                      ShOtaProtocol::statusName(verdict));
        reply(OtaMessage::Reject, offered, verdict, 0);
        return;
    }

    Update.setMD5(offer.md5);
    running = true;
    session = offered;
    imageSize = offer.size;
    received = 0;
    lastDecile = 0;
    lastPacketAt = millis();
    strncpy(version, offer.version, sizeof(version) - 1);
    version[sizeof(version) - 1] = '\0';

    Serial.printf("[OTA] Receiving %s (%u bytes) from gateway; current %s\n", version,
                  static_cast<unsigned>(imageSize), SH_FIRMWARE_VERSION);
    reply(OtaMessage::Accept, session, OtaStatus::Ok, 0);
}

void OtaReceiver::handleData(const OtaPacket& packet) {
    lastPacketAt = millis();
    const uint32_t offset = packet.header.offset;
    const uint16_t length = packet.header.length;

    if (offset == received && length > 0) {
        if (received + length > imageSize) {
            stop(OtaStatus::WriteFailed, true);
            return;
        }
        // Updater::write takes a non-const buffer.
        uint8_t chunk[SH_OTA_CHUNK_SIZE];
        memcpy(chunk, packet.data, length);
        if (Update.write(chunk, length) != length) {
            Serial.println("[OTA] Flash write failed");
            stop(OtaStatus::WriteFailed, true);
            return;
        }
        received += length;

        const uint8_t decile = static_cast<uint8_t>(static_cast<uint64_t>(received) * 10 / imageSize);
        if (decile != lastDecile) {
            lastDecile = decile;
            Serial.printf("[OTA] %u%%\n", decile * 10);
        }
    }
    // Duplicates and out-of-order chunks just get told where we are.
    reply(OtaMessage::Ack, session, OtaStatus::Ok, received);

    if (received == imageSize) finishImage();
}

void OtaReceiver::finishImage() {
    if (!Update.end()) {
        Serial.println("[OTA] Image verification failed (MD5 mismatch or invalid image)");
        reply(OtaMessage::Complete, session, OtaStatus::VerifyFailed, received);
        running = false;
        return;
    }

    ShOtaRollback::noteInstalled(version);
    Serial.printf("[OTA] Installed %s; rebooting\n", version);
    for (uint8_t i = 0; i < COMPLETE_REPEATS; ++i) {
        reply(OtaMessage::Complete, session, OtaStatus::Ok, received);
        delay(50);
    }
    delay(200);
    ESP.restart();
}

void OtaReceiver::stop(OtaStatus status, bool notifyGateway) {
    if (running) abortUpdater();
    if (notifyGateway) reply(OtaMessage::Abort, session, status, received);
    running = false;
    session = 0;
}

void OtaReceiver::reply(OtaMessage type, uint32_t sessionId, OtaStatus status, uint32_t offset) {
    OtaPacket packet;
    ShOtaProtocol::initPacket(packet, type, sessionId, offset, status);
    // Broadcast, like every node-to-gateway frame (unicast to the gateway is
    // not acknowledged on this hardware).
    transport.sendRaw(transport.getBroadcastAddress(),
                      reinterpret_cast<const uint8_t*>(&packet),
                      ShOtaProtocol::packetSize(packet));
}
