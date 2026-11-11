
#pragma once

#include <Arduino.h>
#include "EspNowCompat.h"
#include "ITransport.h"
#include "NodeRegistration.h"

// Broadcast ESP-NOW transport. Works on ESP32 and ESP8266 (incl. NodeMCU).
class EspNowTransport : public ITransport {
private:
    ReceiveCallback callback = nullptr;
    uint8_t channel;

    // Function-local static: header-only safe and C++11 compatible.
    static EspNowTransport*& instance() {
        static EspNowTransport* current = nullptr;
        return current;
    }

    static void receiveHandler(
        const uint8_t* mac,
        const uint8_t* data,
        int length
    ) {
        EspNowTransport* self = instance();

        if (self && self->callback && length > 0) {
            self->callback(data, static_cast<size_t>(length));
        }
    }

public:
    static const uint8_t* broadcastMac() {
        static const uint8_t mac[6] = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
        };
        return mac;
    }

    explicit EspNowTransport(uint8_t wifiChannel = SH_ESPNOW_CHANNEL)
        : channel(wifiChannel) {}

    bool begin() override {
        instance() = this;

        if (!ShEspNow::begin(channel)) {
            return false;
        }

        ShEspNow::onReceive(receiveHandler);

        return ShEspNow::addPeer(broadcastMac(), channel);
    }

    bool send(
        const uint8_t* data,
        size_t length
    ) override {
        return ShEspNow::send(broadcastMac(), data, length);
    }

    void setReceiveCallback(
        ReceiveCallback cb
    ) override {
        callback = cb;
    }
};
