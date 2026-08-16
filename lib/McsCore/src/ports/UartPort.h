#pragma once

#include <string>

class UartPort
{
public:
    virtual ~UartPort() = default;
    virtual bool available() = 0;
    virtual char read() = 0;
    virtual void write(const std::string& text) = 0;
};
