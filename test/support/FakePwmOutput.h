#pragma once

#include "ports/PwmOutput.h"

class FakePwmOutput : public PwmOutput
{
public:
    void writeDutyCycle(double percent) override
    {
        lastDutyCycle_ = percent;
        writeCallCount_++;
    }

    double lastDutyCycle() const
    {
        return lastDutyCycle_;
    }

    int writeCallCount() const
    {
        return writeCallCount_;
    }

private:
    double lastDutyCycle_ = 0.0;
    int writeCallCount_ = 0;
};
