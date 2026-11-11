
#pragma once

#include <stddef.h>
#include "node_types.h"

using ReceiveCallback = void (*)(
    const MacAddress& sender,
    const NodeFrame& frame
);

// Frames that are not registration NodeFrames (e.g. firmware-transfer
// packets, see OtaProtocol.h). Called from the radio's receive context.
using RawReceiveCallback = void (*)(
    const MacAddress& sender,
    const uint8_t* data,
    size_t length
);

class INodeTransport {
public:
    virtual bool begin(uint8_t channel) = 0;

    virtual bool setChannel(uint8_t channel) = 0;

    virtual bool send(
        const MacAddress& destination,
        const NodeFrame& frame
    ) = 0;

    virtual void setReceiveCallback(
        ReceiveCallback callback
    ) = 0;

    virtual bool sendRaw(
        const MacAddress& destination,
        const uint8_t* data,
        size_t length
    ) = 0;

    virtual void setRawReceiveCallback(
        RawReceiveCallback callback
    ) = 0;

    virtual void update() = 0;

    virtual MacAddress getBroadcastAddress() = 0;

    virtual ~INodeTransport() = default;
};
