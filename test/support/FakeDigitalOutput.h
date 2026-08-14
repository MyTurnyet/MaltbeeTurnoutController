#pragma once

#include "ports/DigitalOutput.h"

class FakeDigitalOutput : public DigitalOutput
{
public:
    void set(bool state) override
    {
        state_ = state;
        setCallCount_++;
    }

    bool isSet() const
    {
        return state_;
    }

    int setCallCount() const
    {
        return setCallCount_;
    }

private:
    bool state_ = false;
    int setCallCount_ = 0;
};
