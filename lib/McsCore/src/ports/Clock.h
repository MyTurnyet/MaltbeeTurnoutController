#pragma once

class Clock
{
public:
    virtual ~Clock() = default;
    virtual unsigned long nowMillis() const = 0;
};
