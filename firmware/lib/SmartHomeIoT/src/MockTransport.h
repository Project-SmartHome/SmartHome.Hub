
#pragma once

#include <Arduino.h>
#include "ITransport.h"

class MockTransport : public ITransport {
private:
    ReceiveCallback callback = nullptr;

public:
    bool begin() override {
        Serial.println("[MOCK] Transport started");
        return true;
    }

    bool send(
        const uint8_t* data,
        size_t length
    ) override {
        Serial.print("[MOCK] Sending: ");

        for (size_t i = 0; i < length; i++) {
            Serial.write(data[i]);
        }

        Serial.println();
        return true;
    }

    void setReceiveCallback(
        ReceiveCallback cb
    ) override {
        callback = cb;
    }

    // Allows tests to inject a simulated incoming message.
    void simulateReceive(
        const uint8_t* data,
        size_t length
    ) {
        if (callback) {
            callback(data, length);
        }
    }
};
