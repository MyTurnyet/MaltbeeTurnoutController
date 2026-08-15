#pragma once

#include "ports/Clock.h"
#include "domain/Duration.h"

class FakeClock : public Clock
{
public:
    Instant now() const override
    {
        return currentInstant_;
    }

    void setNow(Instant instant)
    {
        currentInstant_ = instant;
    }

    void advanceBy(Duration duration)
    {
        currentInstant_ = currentInstant_ + duration;
    }

private:
    Instant currentInstant_ = Instant(0);
};
