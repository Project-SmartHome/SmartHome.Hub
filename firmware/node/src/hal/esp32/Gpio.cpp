
#include "Gpio.h"

#include <Arduino.h>

void Gpio::configure(
    uint8_t pin,
    PinMode mode
) {

    switch (mode) {

        case PinMode::Input:
            pinMode(pin, INPUT);
            break;

        case PinMode::InputPullup:
            pinMode(pin, INPUT_PULLUP);
            break;

        case PinMode::Output:
            pinMode(pin, OUTPUT);
            break;
    }
}

void Gpio::write(
    uint8_t pin,
    PinLevel level
) {

    digitalWrite(
        pin,
        level == PinLevel::High ? HIGH : LOW
    );
}

PinLevel Gpio::read(uint8_t pin) {

    return digitalRead(pin) == HIGH
        ? PinLevel::High
        : PinLevel::Low;
}
