#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/DigitalInput.h"
#include "domain/Level.h"

class EspDigitalInput : public DigitalInput
{
public:
    EspDigitalInput(int pin, bool hasInternalPullUp) : pin_(pin)
    {
        pinMode(pin_, hasInternalPullUp ? INPUT_PULLUP : INPUT);
    }

    Level read() override
    {
        return digitalRead(pin_) == HIGH ? Level::High : Level::Low;
    }

private:
    int pin_;
};

#endif
