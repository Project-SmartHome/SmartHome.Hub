
#pragma once

#include <Arduino.h>

class ITransport {
public:
    using ReceiveCallback = void (*)(const uint8_t* data, size_t length);

    virtual ~ITransport() = default;

    virtual bool begin() = 0;

    virtual bool send(
        const uint8_t* data,
        size_t length
    ) = 0;

    virtual void setReceiveCallback(
        ReceiveCallback callback
    ) = 0;
};
