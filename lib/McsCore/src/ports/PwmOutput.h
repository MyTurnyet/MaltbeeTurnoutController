#pragma once

class PwmOutput
{
public:
    virtual ~PwmOutput() = default;
    virtual void writeDutyCycle(double percent) = 0;
};
