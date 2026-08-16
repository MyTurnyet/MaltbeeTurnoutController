#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include <string>

#include "ports/UartPort.h"

class EspUartPort : public UartPort
{
public:
    explicit EspUartPort(unsigned long baudRate)
    {
        Serial.begin(baudRate);
    }

    bool available() override
    {
        return Serial.available() > 0;
    }

    char read() override
    {
        return static_cast<char>(Serial.read());
    }

    void write(const std::string& text) override
    {
        Serial.print(text.c_str());
    }
};

#endif
