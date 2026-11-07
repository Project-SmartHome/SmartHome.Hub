
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

void printMac(const uint8_t *mac, char *output) {
    snprintf(
        output, 18,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]
    );
}

void sendRegistrationAck(const uint8_t *mac) {
    char macString[18];
    printMac(mac, macString);

    RegisterAck ack = {};

    ack.version = SH_PROTOCOL_VERSION;
    ack.messageType = SH_MSG_REGISTER_ACK;
    ack.accepted = 1;

    // Generate stable ID from MAC without colons.
    char compactMac[13];

    snprintf(
        compactMac,
        sizeof(compactMac),
        "%02X%02X%02X%02X%02X%02X",
        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]
    );

    snprintf(
        ack.nodeId,
        sizeof(ack.nodeId),
        "sh-node-%s",
        compactMac
    );

    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer = {};

        memcpy(peer.peer_addr, mac, 6);
        peer.channel = SH_ESPNOW_CHANNEL;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;

        if (esp_now_add_peer(&peer) != ESP_OK) {
            Serial.println("{\"event\":\"PEER_ADD_FAILED\"}");
            return;
        }
    }

    esp_err_t result = esp_now_send(
        mac,
        (uint8_t *)&ack,
        sizeof(ack)
    );

    if (result != ESP_OK) {
        Serial.println("{\"event\":\"ACK_SEND_FAILED\"}");
        return;
    }

    Serial.printf(
        "{\"event\":\"NODE_REGISTERED\","
        "\"nodeId\":\"%s\","
        "\"mac\":\"%s\"}\n",
        ack.nodeId,
        macString
    );
}

// Compatible with older ESP-NOW callback API.
void onDataReceived(
    const uint8_t *mac,
    const uint8_t *data,
    int len
) {
    if (len != sizeof(NodeHello)) return;

    NodeHello hello = {};
    memcpy(&hello, data, sizeof(hello));

    if (hello.version != SH_PROTOCOL_VERSION ||
        hello.messageType != SH_MSG_NODE_HELLO) {
        return;
    }

    if (hello.componentCount > SH_MAX_COMPONENTS) {
        Serial.println("{\"event\":\"INVALID_COMPONENT_COUNT\"}");
        return;
    }

    char macString[18];
    printMac(mac, macString);

    Serial.printf(
        "{\"event\":\"NODE_DISCOVERED\","
        "\"mac\":\"%s\","
        "\"deviceName\":\"%s\","
        "\"componentCount\":%u}\n",
        macString,
        hello.deviceName,
        hello.componentCount
    );

    for (uint8_t i = 0; i < hello.componentCount; i++) {
        Serial.printf(
            "{\"event\":\"COMPONENT_DISCOVERED\","
            "\"mac\":\"%s\","
            "\"componentId\":\"%s\","
            "\"componentType\":%u,"
            "\"direction\":%u}\n",
            macString,
            hello.components[i].id,
            hello.components[i].type,
            hello.components[i].direction
        );
    }

    sendRegistrationAck(mac);
}

void setup() {
    Serial.begin(115200);

    if (!transport.begin()) {
        Serial.println("Transport initialization failed");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_wifi_set_channel(
        SH_ESPNOW_CHANNEL,
        WIFI_SECOND_CHAN_NONE
    );

    Serial.println("SmartHome ESP32 Gateway Starting");

    Serial.print("Gateway MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW initialization failed");
        return;
    }

    esp_now_register_recv_cb(onDataReceived);

    Serial.println("Gateway ready");
}

void loop() {
    delay(10);
}
