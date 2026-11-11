
#pragma once

#include <stdint.h>

enum class PinMode : uint8_t {
    Input,
    InputPullup,
    Output
};

enum class PinLevel : uint8_t {
    Low,
    High
};

class INodeGpio {
public:
    virtual void configure(
        uint8_t pin,
        PinMode mode
    ) = 0;

    virtual void write(
        uint8_t pin,
        PinLevel level
    ) = 0;

    virtual PinLevel read(
        uint8_t pin
    ) = 0;

    virtual ~INodeGpio() = default;
};
