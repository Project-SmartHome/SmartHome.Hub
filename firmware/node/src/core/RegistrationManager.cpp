#include "RegistrationManager.h"

#include <Arduino.h>
#if defined(ESP8266)
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
#else
    #error "Unsupported platform"
#endif
#include <string.h>
#include <OtaConfig.h>   // SH_FIRMWARE_VERSION / SH_FIRMWARE_TARGET

#ifndef SH_ESPNOW_CHANNEL
#define SH_ESPNOW_CHANNEL 6
#endif

#define DISCOVERY_INTERVAL_MS       200UL
#define CHANNEL_DWELL_MS            700UL
#define REGISTRATION_RETRY_MS      5000UL
#define HEARTBEAT_INTERVAL_MS     30000UL
#define PENDING_RETRY_MS          30000UL
#define MAX_UNANSWERED_SENDS          3
#define CONFIG_VERSION 1UL

RegistrationManager* RegistrationManager::instance = nullptr;

namespace {
void copyString(char* destination, size_t capacity, const char* source) {
    if (capacity == 0) return;
    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}
}

RegistrationManager::RegistrationManager(
    INodeTransport& transport,
    INodeStorage& storage
)
    : transport(transport), storage(storage) {}

bool RegistrationManager::begin() {
    Serial.println("[Registration] Starting...");
    instance = this;

    if (!storage.begin()) {
        Serial.println("[Registration] Storage initialization failed");
        return false;
    }

    loadConfiguration();
    initializeIdentity();

    if (!transport.begin(SH_ESPNOW_CHANNEL)) {
        Serial.println("[Registration] ESP-NOW initialization failed");
        return false;
    }

    scanChannel = SH_ESPNOW_CHANNEL;
    lastChannelChangeAt = millis();
    transport.setReceiveCallback(receiveCallback);
    state = config.registered
        ? RegistrationState::Registered
        : RegistrationState::Unknown;

    Serial.print("[Registration] Device ID: ");
    Serial.println(config.deviceId);
    Serial.println(state == RegistrationState::Registered
        ? "[Registration] Previously registered"
        : "[Registration] Searching for gateway");

    sendDiscovery();
    return true;
}

void RegistrationManager::initializeIdentity() {
    uint8_t mac[6] = {};
    WiFi.macAddress(mac);

    char macText[18];
    snprintf(macText, sizeof(macText), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    copyString(config.macAddress, sizeof(config.macAddress), macText);

    if (config.deviceId[0] == '\0') {
        snprintf(config.deviceId, sizeof(config.deviceId),
                 "sh-%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    copyString(config.firmwareVersion, sizeof(config.firmwareVersion),
               SH_FIRMWARE_VERSION);
    config.configVersion = CONFIG_VERSION;

    copyString(nodeInfo.deviceId, sizeof(nodeInfo.deviceId), config.deviceId);
    copyString(nodeInfo.macAddress, sizeof(nodeInfo.macAddress), config.macAddress);
    copyString(nodeInfo.firmwareVersion, sizeof(nodeInfo.firmwareVersion),
               config.firmwareVersion);
    copyString(nodeInfo.homeId, sizeof(nodeInfo.homeId), config.homeId);
    copyString(nodeInfo.roomId, sizeof(nodeInfo.roomId), config.roomId);
    nodeInfo.registered = config.registered;

    saveConfiguration();
}

bool RegistrationManager::loadConfiguration() {
    NodeConfiguration stored{};
    if (!storage.load(stored) || stored.configVersion != CONFIG_VERSION) {
        Serial.println("[Registration] No compatible saved configuration");
        config = {};
        return false;
    }

    config = stored;
    config.deviceId[sizeof(config.deviceId) - 1] = '\0';
    config.homeId[sizeof(config.homeId) - 1] = '\0';
    config.roomId[sizeof(config.roomId) - 1] = '\0';
    Serial.println("[Registration] Configuration restored");
    return true;
}

bool RegistrationManager::saveConfiguration() {
    config.configVersion = CONFIG_VERSION;
    if (!storage.save(config)) {
        Serial.println("[Registration] Configuration save failed");
        return false;
    }
    return true;
}

void RegistrationManager::sendDiscovery() {
    NodeFrame frame{};
    frame.magic = SH_PROTOCOL_MAGIC;
    frame.protocolVersion = SH_PROTOCOL_VERSION;
    frame.messageType = static_cast<uint8_t>(MessageType::Discovery);
    frame.sequence = ++sequence;
    fillIdentity(frame);

    transport.send(transport.getBroadcastAddress(), frame);
    lastDiscoveryAt = millis();
}

void RegistrationManager::requestRegistration(const NodeInfo& info) {
    nodeInfo = info;
    copyString(config.deviceId, sizeof(config.deviceId), info.deviceId);
    copyString(config.homeId, sizeof(config.homeId), info.homeId);
    copyString(config.roomId, sizeof(config.roomId), info.roomId);
    config.registered = info.registered;
    saveConfiguration();

    if (gatewayDiscovered) sendRegistrationRequest();
    else sendDiscovery();
}

void RegistrationManager::sendRegistrationRequest() {
    if (!gatewayDiscovered) return;

    NodeFrame frame{};
    frame.magic = SH_PROTOCOL_MAGIC;
    frame.protocolVersion = SH_PROTOCOL_VERSION;
    frame.messageType = static_cast<uint8_t>(MessageType::RegisterRequest);
    frame.sequence = ++sequence;
    fillIdentity(frame);
    copyString(frame.homeId, sizeof(frame.homeId), config.homeId);
    copyString(frame.roomId, sizeof(frame.roomId), config.roomId);

    sendToGateway(frame);
    Serial.println("[Registration] Request sent to gateway");
    state = RegistrationState::Pending;
    lastRegistrationAt = millis();
}

void RegistrationManager::receiveCallback(
    const MacAddress& sender,
    const NodeFrame& frame
) {
    if (instance == nullptr || frame.payloadLength > SH_MAX_PAYLOAD) return;
    instance->receivedFrame = frame;
    instance->receivedMac = sender;
    instance->frameReceived = true;
}

void RegistrationManager::handleFrame(
    const MacAddress& mac,
    const NodeFrame& frame
) {
    const MessageType type = static_cast<MessageType>(frame.messageType);
    if (gatewayDiscovered && type != MessageType::GatewayHello) {
        if (memcmp(mac.bytes, gatewayMac.bytes, sizeof(mac.bytes)) != 0) return;
        onGatewayReply();
    }

    switch (type) {
        case MessageType::GatewayHello:
            if (gatewayDiscovered) break;  // Duplicate replies to discovery.
            gatewayMac = mac;
            gatewayDiscovered = true;
            onGatewayReply();
            // The reply may have leaked in from an adjacent channel; move to
            // the channel the gateway says it actually operates on.
            if (frame.payloadLength >= 1 &&
                frame.payload[0] >= 1 && frame.payload[0] <= 14 &&
                frame.payload[0] != scanChannel) {
                Serial.printf("[Registration] Heard gateway on channel %u; switching to its channel %u\n",
                              scanChannel, frame.payload[0]);
                scanChannel = frame.payload[0];
                transport.setChannel(scanChannel);
            }
            Serial.printf("[Registration] Gateway %02X:%02X:%02X:%02X:%02X:%02X discovered on channel %u\n",
                          mac.bytes[0], mac.bytes[1], mac.bytes[2],
                          mac.bytes[3], mac.bytes[4], mac.bytes[5], scanChannel);
            if (state == RegistrationState::Registered) {
                sendHeartbeat();
            } else if (state == RegistrationState::Unknown ||
                       (state == RegistrationState::Pending &&
                        millis() - lastRegistrationAt >= REGISTRATION_RETRY_MS)) {
                sendRegistrationRequest();
            }
            break;

        case MessageType::RegisterPending:
            onRegistrationResponse("pending", nullptr, nullptr);
            break;

        case MessageType::Registered:
            onRegistrationResponse("registered", frame.homeId, frame.roomId);
            break;

        case MessageType::RegisterRejected:
            onRegistrationResponse("rejected", nullptr, nullptr);
            break;

        case MessageType::HeartbeatAck:
            Serial.println("[Registration] Heartbeat acknowledged");
            break;

        case MessageType::ConfigUpdate:
            copyString(config.homeId, sizeof(config.homeId), frame.homeId);
            copyString(config.roomId, sizeof(config.roomId), frame.roomId);
            saveConfiguration();
            break;

        default:
            break;
    }
}

void RegistrationManager::onRegistrationResponse(
    const char* status,
    const char* homeId,
    const char* roomId
) {
    if (status == nullptr) return;

    if (strcmp(status, "registered") == 0) {
        state = RegistrationState::Registered;
        config.registered = true;
        if (homeId != nullptr) copyString(config.homeId, sizeof(config.homeId), homeId);
        if (roomId != nullptr) copyString(config.roomId, sizeof(config.roomId), roomId);
        Serial.println("[Registration] Device registered by cloud");
        Serial.print("[Registration] Home: "); Serial.println(config.homeId);
        Serial.print("[Registration] Room: "); Serial.println(config.roomId);
    } else if (strcmp(status, "pending") == 0) {
        state = RegistrationState::Pending;
        config.registered = false;
        Serial.println("[Registration] Cloud approval pending");
    } else if (strcmp(status, "rejected") == 0) {
        state = RegistrationState::Rejected;
        config.registered = false;
        Serial.println("[Registration] Registration rejected by cloud");
    } else {
        return;
    }

    saveConfiguration();
}

void RegistrationManager::sendHeartbeat() {
    if (!gatewayDiscovered || state != RegistrationState::Registered) return;

    NodeFrame frame{};
    frame.magic = SH_PROTOCOL_MAGIC;
    frame.protocolVersion = SH_PROTOCOL_VERSION;
    frame.messageType = static_cast<uint8_t>(MessageType::Heartbeat);
    frame.sequence = ++sequence;
    // Version + target let the gateway decide whether this node needs an update.
    fillIdentity(frame);
    copyString(frame.homeId, sizeof(frame.homeId), config.homeId);
    copyString(frame.roomId, sizeof(frame.roomId), config.roomId);
    sendToGateway(frame);
    lastHeartbeatAt = millis();
}

void RegistrationManager::sendToGateway(const NodeFrame& frame) {
    // Broadcast: the gateway reliably receives these, whereas unicast frames
    // to its MAC are never acknowledged. Other nodes ignore node-originated
    // message types, and the gateway replies by unicast, which does work.
    if (!transport.send(transport.getBroadcastAddress(), frame)) {
        Serial.println("[Registration] ESP-NOW send could not be queued");
    }
    awaitingReply = true;
}

void RegistrationManager::onGatewayReply() {
    awaitingReply = false;
    unansweredSends = 0;
    heardFromGateway = true;
}

// Identity fields shared by every node-originated frame. The payload carries
// the firmware target ("node-<env>") so the gateway can pick the right OTA
// image; gateways without OTA support ignore it.
void RegistrationManager::fillIdentity(NodeFrame& frame) const {
    copyString(frame.deviceId, sizeof(frame.deviceId), config.deviceId);
    copyString(frame.macAddress, sizeof(frame.macAddress), config.macAddress);
    copyString(frame.firmwareVersion, sizeof(frame.firmwareVersion),
               config.firmwareVersion);
    const size_t targetLength = strlen(SH_FIRMWARE_TARGET);
    frame.payloadLength = static_cast<uint8_t>(
        targetLength < SH_MAX_PAYLOAD ? targetLength : SH_MAX_PAYLOAD);
    memcpy(frame.payload, SH_FIRMWARE_TARGET, frame.payloadLength);
}

// Called before a scheduled resend. Returns false (and restarts discovery)
// once the gateway has ignored too many frames in a row.
bool RegistrationManager::noteUnansweredSend() {
    if (!awaitingReply) return true;
    ++unansweredSends;
    Serial.printf("[Registration] No reply from gateway (%u/%u)\n",
                  unansweredSends, MAX_UNANSWERED_SENDS);
    if (unansweredSends < MAX_UNANSWERED_SENDS) return true;
    restartDiscovery();
    return false;
}

void RegistrationManager::restartDiscovery() {
    gatewayDiscovered = false;
    awaitingReply = false;
    unansweredSends = 0;
    lastChannelChangeAt = millis();
    Serial.println("[Registration] Gateway unreachable; searching again");
    sendDiscovery();
}

void RegistrationManager::update() {
    transport.update();

    if (frameReceived) {
        const NodeFrame frame = receivedFrame;
        const MacAddress mac = receivedMac;
        frameReceived = false;
        handleFrame(mac, frame);
    }

    const uint32_t now = millis();
    if (!gatewayDiscovered) {
        if (now - lastChannelChangeAt >= CHANNEL_DWELL_MS) {
            scanChannel = scanChannel >= 13 ? 1 : scanChannel + 1;
            transport.setChannel(scanChannel);
            lastChannelChangeAt = now;
            sendDiscovery();
        } else if (now - lastDiscoveryAt >= DISCOVERY_INTERVAL_MS) {
            sendDiscovery();
        }
        return;
    }

    if (state == RegistrationState::Unknown ||
        state == RegistrationState::Pending) {
        // Retry quickly until the gateway confirms it has the request, then
        // only refresh it occasionally while the cloud decides.
        const uint32_t retryMs = awaitingReply || state == RegistrationState::Unknown
            ? REGISTRATION_RETRY_MS
            : PENDING_RETRY_MS;
        if (now - lastRegistrationAt >= retryMs && noteUnansweredSend()) {
            sendRegistrationRequest();
        }
    }

    if (state == RegistrationState::Registered &&
        now - lastHeartbeatAt >= HEARTBEAT_INTERVAL_MS &&
        noteUnansweredSend()) {
        sendHeartbeat();
    }
}

RegistrationState RegistrationManager::getState() const {
    return state;
}