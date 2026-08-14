#pragma once

#include "ports/Clock.h"

class FakeClock : public Clock
{
public:
    unsigned long nowMillis() const override
    {
        return currentMillis_;
    }

    void setNow(unsigned long milliseconds)
    {
        currentMillis_ = milliseconds;
    }

    void advanceBy(unsigned long milliseconds)
    {
        currentMillis_ += milliseconds;
    }

private:
    unsigned long currentMillis_ = 0;
};
