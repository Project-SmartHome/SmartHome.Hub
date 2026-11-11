
#pragma once

#include "../INodeGpio.h"

class Gpio : public INodeGpio {
public:
    void configure(
        uint8_t pin,
        PinMode mode
    ) override;

    void write(
        uint8_t pin,
        PinLevel level
    ) override;

    PinLevel read(
        uint8_t pin
    ) override;
};
