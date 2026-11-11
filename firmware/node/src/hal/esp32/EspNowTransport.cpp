
#include "EspNowTransport.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

ReceiveCallback EspNowTransport::receiveCallback = nullptr;
RawReceiveCallback EspNowTransport::rawReceiveCallback = nullptr;


bool EspNowTransport::begin(uint8_t channel) {

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_wifi_set_channel(
        channel,
        WIFI_SECOND_CHAN_NONE
    ) != ESP_OK) {
        return false;
    }

    if (esp_now_init() != ESP_OK) {
        return false;
    }

    if (esp_now_register_recv_cb(onReceive) != ESP_OK) {
        return false;
    }

    MacAddress broadcast = getBroadcastAddress();

    return addPeer(broadcast);
}

bool EspNowTransport::setChannel(uint8_t channel) {
    return esp_wifi_set_channel(
        channel,
        WIFI_SECOND_CHAN_NONE
    ) == ESP_OK;
}

bool EspNowTransport::addPeer(
    const MacAddress& mac
) {

    if (esp_now_is_peer_exist(mac.bytes)) {
        return true;
    }

    esp_now_peer_info_t peer{};

    memcpy(peer.peer_addr, mac.bytes, 6);

    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    return esp_now_add_peer(&peer) == ESP_OK;
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
        destination.bytes,
        reinterpret_cast<const uint8_t*>(&frame),
        sizeof(NodeFrame)
    ) == ESP_OK;
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

    if (data == nullptr || length == 0 || length > ESP_NOW_MAX_DATA_LEN) {
        return false;
    }

    if (!addPeer(destination)) {
        return false;
    }

    return esp_now_send(destination.bytes, data, length) == ESP_OK;
}

void EspNowTransport::setRawReceiveCallback(
    RawReceiveCallback callback
) {
    rawReceiveCallback = callback;
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void EspNowTransport::onReceive(
    const esp_now_recv_info_t* info,
    const uint8_t* data,
    int length
) {
    if (info) handleReceive(info->src_addr, data, length);
}
#else
void EspNowTransport::onReceive(
    const uint8_t* mac,
    const uint8_t* data,
    int length
) {
    handleReceive(mac, data, length);
}
#endif

void EspNowTransport::handleReceive(
    const uint8_t* mac,
    const uint8_t* data,
    int length
) {

    if (!mac || !data || length <= 0) {
        return;
    }

    uint16_t magic = 0;
    if (length >= static_cast<int>(sizeof(magic))) {
        memcpy(&magic, data, sizeof(magic));
    }

    if (magic != SH_PROTOCOL_MAGIC ||
        length != sizeof(NodeFrame)) {
        // Not a registration frame: hand it to the raw listener (OTA).
        if (rawReceiveCallback) {
            MacAddress sender;
            memcpy(sender.bytes, mac, 6);
            rawReceiveCallback(sender, data, static_cast<size_t>(length));
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
    // Receive processing currently uses callback delivery.
}

MacAddress EspNowTransport::getBroadcastAddress() {

    MacAddress broadcast = {
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
    };

    return broadcast;
}
