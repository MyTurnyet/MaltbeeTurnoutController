#pragma once

class DigitalOutput
{
public:
    virtual ~DigitalOutput() = default;
    virtual void set(bool state) = 0;
};
