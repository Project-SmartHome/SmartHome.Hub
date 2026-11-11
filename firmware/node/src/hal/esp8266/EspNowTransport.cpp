
#include "EspNowTransport.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <user_interface.h>
#include <string.h>

ReceiveCallback EspNowTransport::receiveCallback = nullptr;
RawReceiveCallback EspNowTransport::rawReceiveCallback = nullptr;

bool EspNowTransport::begin(uint8_t channel) {

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    wifi_set_channel(channel);

    if (esp_now_init() != 0) {
        return false;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

    esp_now_register_recv_cb(onReceive);

    MacAddress broadcast = getBroadcastAddress();

    addPeer(broadcast);

    return true;
}

bool EspNowTransport::setChannel(uint8_t channel) {
    wifi_promiscuous_enable(1);
    wifi_set_channel(channel);
    wifi_promiscuous_enable(0);
    return true;
}

bool EspNowTransport::addPeer(
    const MacAddress& mac
) {

    uint8_t* peer = const_cast<uint8_t*>(mac.bytes);
    const uint8_t channel = wifi_get_channel();

    if (esp_now_is_peer_exist(peer)) {
        // Keep the peer on the radio's current channel after a channel change.
        esp_now_set_peer_channel(peer, channel);
        return true;
    }

    return esp_now_add_peer(
        peer,
        ESP_NOW_ROLE_COMBO,
        channel,
        nullptr,
        0
    ) == 0;
}

bool EspNowTransport::send(
    const MacAddress& destination,
    const NodeFrame& frame
) {

    if (frame.magic != SH_PROTOCOL_MAGIC ||
        frame.protocolVersion != SH_PROTOCOL_VERSION) {
        return false;
    }

    if (frame.payloadLength > SH_MAX_PAYLOAD) {
        return false;
    }

    if (!addPeer(destination)) {
        return false;
    }

    return esp_now_send(
        const_cast<uint8_t*>(destination.bytes),
        reinterpret_cast<uint8_t*>(
            const_cast<NodeFrame*>(&frame)
        ),
        sizeof(NodeFrame)
    ) == 0;
}

void EspNowTransport::setReceiveCallback(
    ReceiveCallback callback
) {
    receiveCallback = callback;
}

bool EspNowTransport::sendRaw(
    const MacAddress& destination,
    const uint8_t* data,
    size_t length
) {

    // ESP-NOW payloads are limited to 250 bytes.
    if (data == nullptr || length == 0 || length > 250) {
        return false;
    }

    if (!addPeer(destination)) {
        return false;
    }

    return esp_now_send(
        const_cast<uint8_t*>(destination.bytes),
        const_cast<uint8_t*>(data),
        static_cast<int>(length)
    ) == 0;
}

void EspNowTransport::setRawReceiveCallback(
    RawReceiveCallback callback
) {
    rawReceiveCallback = callback;
}

void EspNowTransport::onReceive(
    uint8_t* mac,
    uint8_t* data,
    uint8_t length
) {

    if (!mac || !data || length == 0) {
        return;
    }

    uint16_t magic = 0;
    if (length >= sizeof(magic)) {
        memcpy(&magic, data, sizeof(magic));
    }

    if (magic != SH_PROTOCOL_MAGIC ||
        length != sizeof(NodeFrame)) {
        // Not a registration frame: hand it to the raw listener (OTA).
        if (rawReceiveCallback) {
            MacAddress sender;
            memcpy(sender.bytes, mac, 6);
            rawReceiveCallback(sender, data, length);
        }
        return;
    }

    NodeFrame frame;

    memcpy(&frame, data, sizeof(frame));

    if (frame.magic != SH_PROTOCOL_MAGIC ||
        frame.protocolVersion != SH_PROTOCOL_VERSION ||
        frame.payloadLength > SH_MAX_PAYLOAD) {
        return;
    }

    MacAddress sender;

    memcpy(sender.bytes, mac, 6);

    if (receiveCallback) {
        receiveCallback(sender, frame);
    }
}

void EspNowTransport::update() {
    // ESP8266 ESP-NOW callbacks are delivered
    // asynchronously by the Wi-Fi stack.
}

MacAddress EspNowTransport::getBroadcastAddress() {

    MacAddress broadcast = {
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
    };

    return broadcast;
}
