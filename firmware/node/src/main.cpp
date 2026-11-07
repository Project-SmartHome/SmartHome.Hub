
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <NodeRegistration.h>

#include <ITransport.h>

#if defined(SH_TRANSPORT_MOCK)
    #include <MockTransport.h>
    MockTransport transport;
#elif defined(SH_TRANSPORT_ESPNOW)
    #include <EspNowTransport.h>
    EspNowTransport transport;
#else
    #error "No transport selected"
#endif

static const uint8_t BROADCAST_MAC[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// Node configuration
#define DEVICE_NAME "garden-flow-01"

bool registered = false;
char assignedNodeId[24] = {0};

unsigned long lastHello = 0;

const unsigned long DISCOVERY_INTERVAL = 10000;

// Components installed on this node
void configureComponents(NodeHello &hello) {

    hello.componentCount = 1;

    // Water flow sensor
    strncpy(
        hello.components[0].id,
        "flow-01",
        sizeof(hello.components[0].id) - 1
    );

    hello.components[0].type = COMPONENT_WATER_FLOW;
    hello.components[0].direction = DIRECTION_INPUT;
}

// Older ESP32 Arduino framework callback
void onDataReceived(
    const uint8_t *mac,
    const uint8_t *data,
    int len
) {
    if (len != sizeof(RegisterAck)) {
        return;
    }

    RegisterAck ack = {};
    memcpy(&ack, data, sizeof(ack));

    if (ack.version != SH_PROTOCOL_VERSION ||
        ack.messageType != SH_MSG_REGISTER_ACK ||
        !ack.accepted) {
        return;
    }

    strncpy(
        assignedNodeId,
        ack.nodeId,
        sizeof(assignedNodeId) - 1
    );

    assignedNodeId[sizeof(assignedNodeId) - 1] = '\0';

    registered = true;

    Serial.printf(
        "[REGISTRATION] Registered as %s\n",
        assignedNodeId
    );
}

void sendHello() {

    NodeHello hello = {};

    hello.version = SH_PROTOCOL_VERSION;
    hello.messageType = SH_MSG_NODE_HELLO;

    strncpy(
        hello.deviceName,
        DEVICE_NAME,
        sizeof(hello.deviceName) - 1
    );

    configureComponents(hello);

    esp_err_t result = esp_now_send(
        BROADCAST_MAC,
        (uint8_t *)&hello,
        sizeof(hello)
    );

    if (result == ESP_OK) {
        Serial.println("[DISCOVERY] NODE_HELLO sent");
    } else {
        Serial.printf(
            "[DISCOVERY] Send failed: %d\n",
            result
        );
    }
}

void setup() {

    Serial.begin(115200);
    delay(500);

    if (!transport.begin()) {
        Serial.println("Transport initialization failed");
        return;
    }

    Serial.println();
    Serial.println("SmartHome ESP32 Node Starting");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_wifi_set_channel(
        SH_ESPNOW_CHANNEL,
        WIFI_SECOND_CHAN_NONE
    );

    Serial.print("Node MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW initialization failed");
        return;
    }

    esp_now_register_recv_cb(onDataReceived);

    esp_now_peer_info_t peer = {};

    memcpy(peer.peer_addr, BROADCAST_MAC, 6);

    peer.channel = SH_ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    if (!esp_now_is_peer_exist(BROADCAST_MAC)) {

        if (esp_now_add_peer(&peer) != ESP_OK) {
            Serial.println("Failed to add broadcast peer");
            return;
        }
    }

    Serial.println("Node initialized successfully");

    sendHello();

    lastHello = millis();
}

void loop() {

    // Retry discovery until registration succeeds.
    if (!registered &&
        millis() - lastHello >= DISCOVERY_INTERVAL) {

        sendHello();
        lastHello = millis();
    }

    if (registered) {
        // Sensor and actuator logic goes here.
        // Use assignedNodeId for telemetry.
    }

    delay(10);
}
