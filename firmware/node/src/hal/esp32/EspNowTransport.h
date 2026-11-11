
#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include "../INodeTransport.h"

class EspNowTransport : public INodeTransport {
public:
    bool begin(uint8_t channel) override;

    bool setChannel(uint8_t channel) override;

    bool send(
        const MacAddress& destination,
        const NodeFrame& frame
    ) override;

    void setReceiveCallback(
        ReceiveCallback callback
    ) override;

    bool sendRaw(
        const MacAddress& destination,
        const uint8_t* data,
        size_t length
    ) override;

    void setRawReceiveCallback(
        RawReceiveCallback callback
    ) override;

    void update() override;

    MacAddress getBroadcastAddress() override;

private:
    static ReceiveCallback receiveCallback;
    static RawReceiveCallback rawReceiveCallback;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    static void onReceive(
        const esp_now_recv_info_t* info,
        const uint8_t* data,
        int length
    );
#else
    static void onReceive(
        const uint8_t* mac,
        const uint8_t* data,
        int length
    );
#endif

    static void handleReceive(
        const uint8_t* mac,
        const uint8_t* data,
        int length
    );

    bool addPeer(const MacAddress& mac);
};
