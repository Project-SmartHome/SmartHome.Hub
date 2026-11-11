
#pragma once

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

    static void onReceive(
        uint8_t* mac,
        uint8_t* data,
        uint8_t length
    );

    bool addPeer(const MacAddress& mac);
};
