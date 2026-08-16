#pragma once

#include <string>

class UartPort
{
public:
    virtual ~UartPort() = default;
    virtual bool available() = 0;
    // Precondition: call only when available() is true; undefined otherwise.
    virtual char read() = 0;
    virtual void write(const std::string& text) = 0;
};
