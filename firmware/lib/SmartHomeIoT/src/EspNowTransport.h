
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "ITransport.h"

class EspNowTransport : public ITransport {
private:
    ReceiveCallback callback = nullptr;

    static EspNowTransport* instance;

    static void receiveHandler(
        const uint8_t* mac,
        const uint8_t* data,
        int length
    ) {
        if (instance && instance->callback) {
            instance->callback(data, length);
        }
    }

public:
    bool begin() override {
        instance = this;

        WiFi.mode(WIFI_STA);

        if (esp_now_init() != ESP_OK) {
            return false;
        }

        esp_now_register_recv_cb(receiveHandler);

        return true;
    }

    bool send(
        const uint8_t* data,
        size_t length
    ) override {
        uint8_t broadcastAddress[] = {
            0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF
        };

        esp_now_peer_info_t peer = {};

        memcpy(
            peer.peer_addr,
            broadcastAddress,
            6
        );

        peer.channel = 0;
        peer.encrypt = false;

        if (!esp_now_is_peer_exist(broadcastAddress)) {
            if (esp_now_add_peer(&peer) != ESP_OK) {
                return false;
            }
        }

        return esp_now_send(
            broadcastAddress,
            data,
            length
        ) == ESP_OK;
    }

    void setReceiveCallback(
        ReceiveCallback cb
    ) override {
        callback = cb;
    }
};

EspNowTransport* EspNowTransport::instance = nullptr;
