#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <string.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <EspNowCompat.h>
#include <NodeRegistration.h>
#include <DeviceSettings.h>
#include <OtaConfig.h>
#include <OtaHttp.h>
#include <OtaProtocol.h>
#include <OtaRollback.h>

#include "NodeOtaRelay.h"

// secrets.h is only present for local USB flashing. Its values are saved to
// flash on boot (see DeviceSettings.h), so OTA images built in CI, which have
// no secrets.h, keep working with the stored credentials.
#if __has_include("secrets.h")
    #include "secrets.h"
#endif
#ifndef WIFI_SSID
    #define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
    #define WIFI_PASSWORD ""
#endif
#ifndef ABLY_KEY_NAME
    #define ABLY_KEY_NAME ""
#endif
#ifndef ABLY_KEY_SECRET
    #define ABLY_KEY_SECRET ""
#endif

#include "../../room-nodes/include/AblyRootCA.h"

#ifndef SH_ESPNOW_CHANNEL
#define SH_ESPNOW_CHANNEL 6
#endif

// The user who owns this gateway. Every registration channel is scoped to
// it: smarthome/<userId>/device-registration/...
#ifndef SH_USER_ID
#define SH_USER_ID ""
#endif

// The Arduino core leaves the post-OTA "is this image good?" decision to us;
// see ShOtaRollback (the image is confirmed once the gateway reaches the
// cloud, or rolled back if it never does).
extern "C" bool verifyRollbackLater() {
    return true;
}

// The Arduino default (8 KB) is not enough: TLS certificate verification
// for firmware downloads runs on this task, several calls deep inside the
// node-update relay, and overflowed it ("Stack canary watchpoint triggered
// (loopTask)"). 16 KB leaves ample headroom; see the log after downloads.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

#ifndef SH_TOPIC_ROOT
#define SH_TOPIC_ROOT "smarthome"
#endif

namespace {
constexpr uint16_t MQTT_PORT = 8883;
constexpr uint16_t MQTT_KEEP_ALIVE_SECONDS = 30;
constexpr uint8_t MAX_PENDING_NODES = 8;
constexpr uint32_t MQTT_RETRY_MIN_MS = 2000;
constexpr uint32_t MQTT_RETRY_MAX_MS = 60000;
constexpr uint32_t PUBLISH_RETRY_MS = 15000;
constexpr time_t VALID_CLOCK_EPOCH = 1700000000;

struct PendingNode {
    bool used = false;
    bool subscribed = false;
    uint8_t mac[6] = {};
    uint16_t sequence = 0;
    uint32_t lastPublishedAt = 0;
    char deviceId[37] = {};
    char requestId[40] = {};
    NodeFrame request{};
};

struct ReceivedPacket {
    uint8_t mac[6] = {};
    NodeFrame frame{};
};

struct ReceivedOtaPacket {
    uint8_t mac[6] = {};
    OtaPacket packet{};
};

constexpr uint8_t OTA_QUEUE_SIZE = 4;

PendingNode pendingNodes[MAX_PENDING_NODES];
ReceivedPacket receiveQueue[8];
volatile uint8_t receiveHead = 0;
volatile uint8_t receiveTail = 0;
ReceivedOtaPacket otaQueue[OTA_QUEUE_SIZE];
volatile uint8_t otaHead = 0;
volatile uint8_t otaTail = 0;
WiFiClientSecure tlsClient;
PubSubClient mqtt(tlsClient);
ShSettings::Settings settings;
NodeOtaRelay nodeOta;
char gatewayId[37] = {};
char registrationRequestTopic[128] = {};
bool userIdValid = false;
char mqttClientId[40] = {};
bool wifiWasConnected = false;
uint32_t lastWifiAttemptAt = 0;
constexpr uint32_t WIFI_RETRY_MS = 30000;
bool clockReadyWasLogged = false;
uint32_t lastMqttAttempt = 0;
uint32_t mqttRetryDelay = MQTT_RETRY_MIN_MS;
uint32_t nextUpdateCheckAt = 0;   // 0 = check as soon as the network is ready
bool updateCheckScheduled = false;

void copyText(char* destination, size_t capacity, const char* source) {
    if (capacity == 0) return;
    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

PendingNode* findPendingByDevice(const char* deviceId) {
    for (PendingNode& pending : pendingNodes) {
        if (pending.used && strcmp(pending.deviceId, deviceId) == 0) {
            return &pending;
        }
    }
    return nullptr;
}

PendingNode* findPendingByRequest(const char* requestId) {
    for (PendingNode& pending : pendingNodes) {
        if (pending.used && strcmp(pending.requestId, requestId) == 0) {
            return &pending;
        }
    }
    return nullptr;
}

PendingNode* allocatePending(const char* deviceId) {
    PendingNode* existing = findPendingByDevice(deviceId);
    if (existing != nullptr) return existing;

    for (PendingNode& pending : pendingNodes) {
        if (!pending.used) {
            pending = PendingNode{};
            pending.used = true;
            copyText(pending.deviceId, sizeof(pending.deviceId), deviceId);
            // Stable and idempotent across retries: the cloud service should
            // use deviceId/requestId as its registration idempotency key.
            copyText(pending.requestId, sizeof(pending.requestId), deviceId);
            return &pending;
        }
    }
    return nullptr;
}

bool sendNodeFrame(const uint8_t* mac, const NodeFrame& frame) {
    const uint8_t channel = WiFi.channel() == 0 ? SH_ESPNOW_CHANNEL : WiFi.channel();
    return ShEspNow::addPeer(mac, channel) &&
           ShEspNow::send(mac, reinterpret_cast<const uint8_t*>(&frame), sizeof(frame));
}

void sendNodeStatus(PendingNode& pending, MessageType type,
                    const char* homeId = nullptr, const char* roomId = nullptr) {
    NodeFrame frame{};
    frame.magic = SH_PROTOCOL_MAGIC;
    frame.protocolVersion = SH_PROTOCOL_VERSION;
    frame.messageType = static_cast<uint8_t>(type);
    frame.sequence = pending.sequence;
    copyText(frame.deviceId, sizeof(frame.deviceId), pending.deviceId);
    if (homeId != nullptr) copyText(frame.homeId, sizeof(frame.homeId), homeId);
    if (roomId != nullptr) copyText(frame.roomId, sizeof(frame.roomId), roomId);

    if (!sendNodeFrame(pending.mac, frame)) {
        Serial.printf("[ESP-NOW] Could not return status for %s\n", pending.deviceId);
    }
}

bool subscribeForResponse(PendingNode& pending) {
    if (pending.subscribed) return true;
    if (!userIdValid) return false;
    char topic[192];
    snprintf(topic, sizeof(topic), "%s/%s/device-registration/acks/%s",
             SH_TOPIC_ROOT, settings.userId, pending.deviceId);
    pending.subscribed = mqtt.subscribe(topic, 0);
    if (!pending.subscribed) {
        Serial.printf("[Ably] Subscribe failed: %s\n", topic);
    }
    return pending.subscribed;
}

bool publishRegistration(PendingNode& pending, const NodeFrame& request) {
    if (!mqtt.connected() || !userIdValid) return false;
    pending.lastPublishedAt = millis();
    if (!subscribeForResponse(pending)) {
        Serial.println("[Ably] Ack subscription unavailable; checking key capability");
    }

    JsonDocument document;
    document["event"] = "device.register.request";
    document["requestId"] = pending.requestId;
    document["deviceId"] = pending.deviceId;
    document["mac"] = request.macAddress;
    document["firmwareVersion"] = request.firmwareVersion;
    document["homeId"] = request.homeId;
    document["roomId"] = request.roomId;
    document["gatewayId"] = gatewayId;
    document["userId"] = settings.userId;

    char payload[512];
    const size_t length = serializeJson(document, payload, sizeof(payload));
    if (length == 0 || length >= sizeof(payload)) return false;

    if (!mqtt.publish(registrationRequestTopic,
                      reinterpret_cast<const uint8_t*>(payload), length, false)) {
        Serial.printf("[Ably] Registration publish failed for %s\n", pending.deviceId);
        return false;
    }

    Serial.printf("[Ably] Registration request for %s published to %s\n",
                  pending.deviceId, registrationRequestTopic);
    return true;
}

void onCloudMessage(char* topic, byte* payload, unsigned int length) {
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, payload, length);
    if (error) {
        Serial.printf("[Ably] Ignoring invalid acknowledgement: %s\n", error.c_str());
        return;
    }

    const char* requestId = document["requestId"] | "";
    const char* status = document["status"] | "";
    const char* homeId = document["homeId"] | "";
    const char* roomId = document["roomId"] | "";
    PendingNode* pending = findPendingByRequest(requestId);
    if (pending == nullptr || requestId[0] == '\0') {
        Serial.printf("[Ably] Acknowledgement has no pending device (topic %s)\n", topic);
        return;
    }

    if (strcmp(status, "pending") == 0) {
        sendNodeStatus(*pending, MessageType::RegisterPending);
        return;
    }
    if (strcmp(status, "registered") == 0) {
        sendNodeStatus(*pending, MessageType::Registered, homeId, roomId);
        Serial.printf("[Registration] Cloud approved %s\n", pending->deviceId);
        pending->used = false;
        return;
    }
    if (strcmp(status, "rejected") == 0) {
        sendNodeStatus(*pending, MessageType::RegisterRejected);
        Serial.printf("[Registration] Cloud rejected %s\n", pending->deviceId);
        pending->used = false;
        return;
    }

    Serial.printf("[Ably] Unknown registration status for %s\n", pending->deviceId);
}

// OTA-capable nodes put their firmware target ("node-<env>") in the payload
// of Heartbeat and RegisterRequest frames, next to firmwareVersion.
void noteNodeFirmware(const uint8_t* mac, const NodeFrame& frame) {
    char target[SH_OTA_TARGET_LENGTH] = {};
    const size_t length = frame.payloadLength < sizeof(target) - 1
        ? frame.payloadLength : sizeof(target) - 1;
    for (size_t i = 0; i < length; ++i) {
        const char c = static_cast<char>(frame.payload[i]);
        if (c <= ' ' || c > '~') return;  // not a target string
        target[i] = c;
    }
    nodeOta.noteNode(mac, frame.deviceId, target, frame.firmwareVersion);
}

void processNodeFrame(const uint8_t* mac, const NodeFrame& incomingFrame) {
    NodeFrame frame = incomingFrame;
    frame.deviceId[sizeof(frame.deviceId) - 1] = '\0';
    frame.macAddress[sizeof(frame.macAddress) - 1] = '\0';
    frame.firmwareVersion[sizeof(frame.firmwareVersion) - 1] = '\0';
    frame.homeId[sizeof(frame.homeId) - 1] = '\0';
    frame.roomId[sizeof(frame.roomId) - 1] = '\0';

    const MessageType type = static_cast<MessageType>(frame.messageType);
    
    Serial.printf("[ESP-NOW] Received type %u from %s (%02X:%02X:%02X:%02X:%02X:%02X)\n",
                  static_cast<unsigned>(type), frame.deviceId,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (type == MessageType::Discovery) {
        NodeFrame hello{};
        hello.magic = SH_PROTOCOL_MAGIC;
        hello.protocolVersion = SH_PROTOCOL_VERSION;
        hello.messageType = static_cast<uint8_t>(MessageType::GatewayHello);
        hello.sequence = frame.sequence;
        copyText(hello.deviceId, sizeof(hello.deviceId), gatewayId);
        // Advertise the real operating channel. Nodes scanning nearby channels
        // can decode this reply through adjacent-channel leakage, but their
        // acknowledged unicast frames only work on the exact channel.
        const uint8_t channel = WiFi.channel() == 0 ? SH_ESPNOW_CHANNEL : WiFi.channel();
        hello.payloadLength = 1;
        hello.payload[0] = channel;
        Serial.printf("[ESP-NOW] Sending GatewayHello (channel %u) to %s\n",
                      channel, frame.deviceId);
        if (!sendNodeFrame(mac, hello)) {
            Serial.printf("[ESP-NOW] GatewayHello send failed for %s\n",
                          frame.deviceId);
        }
        return;
    }

    if (type == MessageType::Heartbeat) {
        NodeFrame ack{};
        ack.magic = SH_PROTOCOL_MAGIC;
        ack.protocolVersion = SH_PROTOCOL_VERSION;
        ack.messageType = static_cast<uint8_t>(MessageType::HeartbeatAck);
        ack.sequence = frame.sequence;
        copyText(ack.deviceId, sizeof(ack.deviceId), frame.deviceId);
        sendNodeFrame(mac, ack);
        noteNodeFirmware(mac, frame);
        return;
    }

    if (type != MessageType::RegisterRequest || frame.deviceId[0] == '\0') return;

    Serial.printf("[ESP-NOW] Registration request received: %s\n", frame.deviceId);
    noteNodeFirmware(mac, frame);

    PendingNode* pending = allocatePending(frame.deviceId);
    if (pending == nullptr) {
        Serial.println("[Registration] Pending-node capacity reached");
        return;
    }

    memcpy(pending->mac, mac, sizeof(pending->mac));
    pending->sequence = frame.sequence;
    pending->request = frame;
    sendNodeStatus(*pending, MessageType::RegisterPending);

    if (pending->lastPublishedAt != 0) {
        // Node retry for a request already sent to Ably; the loop() retry
        // timer handles republishing, so don't publish on every node retry.
        return;
    }
    if (mqtt.connected()) {
        publishRegistration(*pending, pending->request);
    } else {
        Serial.printf("[Registration] Queued until Ably is connected: %s\n", frame.deviceId);
    }
}

void onEspNowMessage(const uint8_t* mac, const uint8_t* data, int length) {
    if (mac == nullptr || data == nullptr || length <= 0) return;

    // Firmware-transfer replies from nodes (Accept/Ack/Complete...).
    if (ShOtaProtocol::isOtaPacket(data, static_cast<size_t>(length))) {
        const uint8_t next = static_cast<uint8_t>((otaHead + 1) % OTA_QUEUE_SIZE);
        if (next == otaTail) return;  // full; the transfer resends on timeout
        if (!ShOtaProtocol::parsePacket(data, static_cast<size_t>(length), otaQueue[otaHead].packet)) {
            return;
        }
        memcpy(otaQueue[otaHead].mac, mac, 6);
        otaHead = next;
        return;
    }

    if (length != static_cast<int>(sizeof(NodeFrame))) return;

    NodeFrame frame{};
    memcpy(&frame, data, sizeof(frame));
    if (frame.magic != SH_PROTOCOL_MAGIC ||
        frame.protocolVersion != SH_PROTOCOL_VERSION ||
        frame.payloadLength > SH_MAX_PAYLOAD) {
        return;
    }

    const uint8_t next = static_cast<uint8_t>((receiveHead + 1) % 8);
    if (next == receiveTail) return;
    memcpy(receiveQueue[receiveHead].mac, mac, 6);
    receiveQueue[receiveHead].frame = frame;
    receiveHead = next;
}

// Logs why the access point refused or dropped us (runs on the Wi-Fi event
// task). Repeats of the same reason are only logged every 30 s.
void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event != ARDUINO_EVENT_WIFI_STA_DISCONNECTED) return;
    static uint8_t lastReason = 0;
    static uint32_t lastLoggedAt = 0;
    const uint8_t reason = info.wifi_sta_disconnected.reason;
    if (reason == lastReason && millis() - lastLoggedAt < 30000) return;
    lastReason = reason;
    lastLoggedAt = millis();
    Serial.printf("[WiFi] Not connected: reason %u (%s)\n", reason,
                  WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason)));
}

void connectWifi() {
    if (settings.wifiSsid[0] == '\0') {
        Serial.println("[WiFi] No credentials: flash once over USB with gateway/include/secrets.h");
        return;
    }
    WiFi.setAutoReconnect(true);
    WiFi.begin(settings.wifiSsid, settings.wifiPassword);
    lastWifiAttemptAt = millis();
    Serial.printf("[WiFi] Connecting to %s\n", settings.wifiSsid);
}

// The core retries a failed first connection only once, and later only for
// some failure reasons, so without this a gateway that missed the router at
// boot could stay offline (and unable to update) until power-cycled.
void retryWifiIfStuck() {
    if (settings.wifiSsid[0] == '\0' || millis() - lastWifiAttemptAt < WIFI_RETRY_MS) return;
    Serial.printf("[WiFi] Still not connected (status %d); retrying %s\n",
                  static_cast<int>(WiFi.status()), settings.wifiSsid);
    WiFi.disconnect();
    WiFi.begin(settings.wifiSsid, settings.wifiPassword);
    lastWifiAttemptAt = millis();
}

bool clockIsValid() {
    return time(nullptr) >= VALID_CLOCK_EPOCH;
}

void connectMqtt() {
    if (settings.ablyKeyName[0] == '\0' || settings.ablyKeySecret[0] == '\0') {
        Serial.println("[Ably] Configure a gateway Ably key in secrets.h");
        return;
    }

    Serial.printf("[Ably] Connecting as %s\n", mqttClientId);
    if (!mqtt.connect(mqttClientId, settings.ablyKeyName, settings.ablyKeySecret)) {
        mqttRetryDelay = min(mqttRetryDelay * 2, MQTT_RETRY_MAX_MS);
        Serial.printf("[Ably] Connect failed (state %d), retry in %lus; verify key name/secret and network access\n",
                      mqtt.state(), mqttRetryDelay / 1000);
        return;
    }

    mqttRetryDelay = MQTT_RETRY_MIN_MS;
    Serial.println("[Ably] Connected");
    ShOtaRollback::markHealthy();
    for (PendingNode& pending : pendingNodes) {
        if (pending.used) {
            pending.subscribed = false;
            subscribeForResponse(pending);
        }
    }
}

// A user ID becomes one segment of an Ably channel name, so it must not be
// empty or contain separators/wildcards that would change the channel.
bool isValidUserId(const char* userId) {
    const size_t length = strlen(userId);
    if (length == 0 || length > 64) return false;
    for (size_t i = 0; i < length; ++i) {
        const char c = userId[i];
        if (c == '/' || c == '+' || c == '#' || c == ':' || c == '*' ||
            c == '[' || c == ']' || c <= ' ') {
            return false;
        }
    }
    return true;
}

void configureUser() {
    userIdValid = isValidUserId(settings.userId);
    if (!userIdValid) {
        Serial.println("[Gateway] Set a valid SH_USER_ID in secrets.h; registrations will not be published");
        return;
    }
    snprintf(registrationRequestTopic, sizeof(registrationRequestTopic),
             "%s/%s/device-registration/requests", SH_TOPIC_ROOT, settings.userId);
    Serial.printf("[Gateway] User %s; requests go to %s\n",
                  settings.userId, registrationRequestTopic);
}

bool sendRawToNode(const uint8_t* mac, const uint8_t* data, size_t length) {
    const uint8_t channel = WiFi.channel() == 0 ? SH_ESPNOW_CHANNEL : WiFi.channel();
    return ShEspNow::addPeer(mac, channel) && ShEspNow::send(mac, data, length);
}

void scheduleUpdateCheck(uint32_t delayMs) {
    // Up to 2 minutes of jitter so a fleet does not hit GitHub in lockstep.
    nextUpdateCheckAt = millis() + delayMs + (esp_random() % 120000UL);
    updateCheckScheduled = true;
}

// Fetches the release manifest; installs a newer gateway image (and reboots),
// and hands node images to the ESP-NOW relay.
void checkForUpdates() {
#if defined(SH_OTA_DISABLED)
    updateCheckScheduled = true;
    nextUpdateCheckAt = millis() + 0x7FFFFFFFUL;
#else
    Serial.printf("[OTA] Checking %s\n", SH_OTA_MANIFEST_URL);
    JsonDocument manifest;
    String error;
    if (!ShOta::fetchManifest(SH_OTA_MANIFEST_URL, manifest, nullptr, error)) {
        Serial.printf("[OTA] Update check failed: %s\n", error.c_str());
        scheduleUpdateCheck(SH_OTA_RETRY_MINUTES * 60000UL);
        return;
    }
    // Reaching the release server over TLS proves this image can take the
    // next update, which is what rollback protection has to guarantee.
    ShOtaRollback::markHealthy();
    scheduleUpdateCheck(SH_OTA_CHECK_INTERVAL_MINUTES * 60000UL);

    ShOta::Release self;
    nodeOta.clearReleases();
    for (JsonPairConst entry : manifest["targets"].as<JsonObjectConst>()) {
        ShOta::Release release;
        if (!ShOta::readRelease(entry.key().c_str(), entry.value(), release)) continue;
        if (strcmp(release.target, SH_FIRMWARE_TARGET) == 0) {
            self = release;
        } else if (strncmp(release.target, "node-", 5) == 0) {
            nodeOta.addRelease(release);
        }
    }

    if (!self.valid()) {
        Serial.printf("[OTA] Release %s has no image for %s\n",
                      manifest["version"] | "?", SH_FIRMWARE_TARGET);
        return;
    }
    if (!ShFirmware::isNewer(self.version, SH_FIRMWARE_VERSION)) {
        Serial.printf("[OTA] Gateway is up to date (%s, latest %s)\n",
                      SH_FIRMWARE_VERSION, self.version);
        return;
    }
    if (ShOtaRollback::isBad(self.version)) {
        Serial.printf("[OTA] Skipping %s: it was rolled back before\n", self.version);
        return;
    }

    Serial.printf("[OTA] Updating gateway %s -> %s\n", SH_FIRMWARE_VERSION, self.version);
    nodeOta.abortTransfer();
    mqtt.disconnect();
    String installError;
    if (!ShOta::installFromUrl(self, installError)) {
        Serial.printf("[OTA] Gateway update failed: %s\n", installError.c_str());
        scheduleUpdateCheck(SH_OTA_RETRY_MINUTES * 60000UL);
        return;
    }
    ShOtaRollback::noteInstalled(self.version);
    Serial.println("[OTA] Gateway update installed; rebooting");
    delay(500);
    ESP.restart();
#endif
}

void configureGatewayIdentity() {
    const String mac = WiFi.macAddress();
    snprintf(gatewayId, sizeof(gatewayId), "gateway-%s", mac.c_str());
    for (char& character : gatewayId) {
        if (character == ':') character = '-';
    }
    copyText(mqttClientId, sizeof(mqttClientId), gatewayId);
}
} // namespace

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\nSmartHome Gateway Starting");
    Serial.printf("[Gateway] Firmware %s (%s)\n", SH_FIRMWARE_VERSION, SH_FIRMWARE_TARGET);
    ShOtaRollback::begin(SH_FIRMWARE_VERSION);

    const ShSettings::Source source = ShSettings::load(
        ShSettings::fromBuild(WIFI_SSID, WIFI_PASSWORD, ABLY_KEY_NAME, ABLY_KEY_SECRET, SH_USER_ID, ""),
        settings);
    Serial.printf("[Gateway] Credentials from %s\n", ShSettings::sourceName(source));

    WiFi.mode(WIFI_STA);
    // Wi-Fi modem sleep turns the radio off between AP beacons, so the
    // gateway misses most ESP-NOW frames from nodes. Keep the radio awake.
    WiFi.setSleep(false);
    if (!ShEspNow::begin(SH_ESPNOW_CHANNEL)) {
        Serial.println("[ESP-NOW] Initialization failed");
        return;
    }
    ShEspNow::onReceive(onEspNowMessage);
    nodeOta.begin(sendRawToNode);
    configureGatewayIdentity();
    configureUser();
    Serial.printf("[Gateway] STA MAC %s, AP MAC %s\n",
                  WiFi.macAddress().c_str(), WiFi.softAPmacAddress().c_str());

    tlsClient.setCACert(ABLY_ROOT_CA);
    mqtt.setServer("main.mqtt.ably.net", MQTT_PORT);
    mqtt.setKeepAlive(MQTT_KEEP_ALIVE_SECONDS);
    mqtt.setBufferSize(768);
    mqtt.setCallback(onCloudMessage);

    WiFi.onEvent(onWifiEvent);
    connectWifi();
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    Serial.printf("[Gateway] Ready; ESP-NOW will follow Wi-Fi channel %u\n",
                  WiFi.channel());
}

void loop() {
    while (receiveTail != receiveHead) {
        const uint8_t index = receiveTail;
        ReceivedPacket packet = receiveQueue[index];
        receiveTail = static_cast<uint8_t>((receiveTail + 1) % 8);
        processNodeFrame(packet.mac, packet.frame);
    }

    while (otaTail != otaHead) {
        const ReceivedOtaPacket received = otaQueue[otaTail];
        otaTail = static_cast<uint8_t>((otaTail + 1) % OTA_QUEUE_SIZE);
        nodeOta.onPacket(received.mac, received.packet);
    }

    ShOtaRollback::loop(WiFi.status() == WL_CONNECTED);

    // Node transfers run from the cached image and need no internet, so they
    // continue through Wi-Fi/Ably outages; only image downloads wait for it.
    const bool networkReady = WiFi.status() == WL_CONNECTED && clockIsValid();
    nodeOta.loop(networkReady);

    if (WiFi.status() != WL_CONNECTED) {
        if (wifiWasConnected) {
            Serial.println("[WiFi] Disconnected; waiting for reconnect");
            wifiWasConnected = false;
            lastWifiAttemptAt = millis();  // give auto-reconnect the first 30 s
            for (PendingNode& pending : pendingNodes) pending.subscribed = false;
        }
        retryWifiIfStuck();
        delay(nodeOta.busy() ? 1 : 10);
        return;
    }

    if (!wifiWasConnected) {
        wifiWasConnected = true;
        // Re-assert after every association; the Wi-Fi driver can restore
        // its default modem-sleep mode when a connection is established.
        esp_wifi_set_ps(WIFI_PS_NONE);
        Serial.print("[WiFi] Connected, IP ");
        Serial.println(WiFi.localIP());
        Serial.printf("[WiFi] ESP-NOW channel is %u\n", WiFi.channel());
    }

    if (!clockIsValid()) {
        if (!clockReadyWasLogged) {
            Serial.println("[TIME] Waiting for NTP before opening TLS to Ably");
            clockReadyWasLogged = true;
        }
        delay(nodeOta.busy() ? 1 : 10);
        return;
    }

    static bool timeSyncLogged = false;
    if (!timeSyncLogged) {
        Serial.println("[TIME] Clock valid; Ably TLS can start");
        timeSyncLogged = true;
    }

    // Checked before (and independently of) Ably, so a release that breaks
    // the Ably connection can still be replaced by the next one. Not while a
    // node transfer is running: that would stall it for the check's duration.
    if (!nodeOta.busy() &&
        (!updateCheckScheduled || static_cast<int32_t>(millis() - nextUpdateCheckAt) >= 0)) {
        checkForUpdates();
    }

    if (!mqtt.connected()) {
        if (lastMqttAttempt == 0 || millis() - lastMqttAttempt >= mqttRetryDelay) {
            lastMqttAttempt = millis();
            connectMqtt();
        }
        delay(nodeOta.busy() ? 1 : 10);
        return;
    }

    mqtt.loop();
    const uint32_t now = millis();
    for (PendingNode& pending : pendingNodes) {
        if (pending.used &&
            (pending.lastPublishedAt == 0 || now - pending.lastPublishedAt >= PUBLISH_RETRY_MS)) {
            // A compact retry request uses the device ID as the idempotency key.
            publishRegistration(pending, pending.request);
        }
    }
    // Keep the loop tight while streaming firmware to a node.
    delay(nodeOta.busy() ? 1 : 5);
}
