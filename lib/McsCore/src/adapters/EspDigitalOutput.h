#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/DigitalOutput.h"
#include "domain/Level.h"

class EspDigitalOutput : public DigitalOutput
{
public:
    explicit EspDigitalOutput(int pin) : pin_(pin)
    {
        pinMode(pin_, OUTPUT);
    }

    void write(Level level) override
    {
        digitalWrite(pin_, level == Level::High ? HIGH : LOW);
    }

private:
    int pin_;
};

#endif
