
#pragma once

#include <Arduino.h>
#include "ITransport.h"

class UartTransport : public ITransport {
private:
    HardwareSerial& serial;
    ReceiveCallback callback = nullptr;

    String buffer;

public:
    explicit UartTransport(HardwareSerial& uart)
        : serial(uart) {}

    bool begin() override {
#if defined(ESP32)
        serial.begin(115200, SERIAL_8N1, 16, 17);  // RX=16, TX=17
#else
        serial.begin(115200);  // ESP8266 UART pins are fixed
#endif
        return true;
    }

    bool send(
        const uint8_t* data,
        size_t length
    ) override {
        // Newline-delimited messages for simulation.
        for (size_t i = 0; i < length; i++) {
            serial.write(data[i]);
        }

        serial.write('\n');
        return true;
    }

    void setReceiveCallback(
        ReceiveCallback cb
    ) override {
        callback = cb;
    }

    void update() {
        while (serial.available()) {
            char c = serial.read();

            if (c == '\n') {
                if (callback && buffer.length() > 0) {
                    callback(
                        reinterpret_cast<const uint8_t*>(
                            buffer.c_str()
                        ),
                        buffer.length()
                    );
                }

                buffer = "";
            } else if (buffer.length() < 512) {
                buffer += c;
            }
        }
    }
};
