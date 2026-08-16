#pragma once

#include "domain/Instant.h"
#include "domain/Duration.h"

class Deadline
{
public:
    void arm(Instant now, Duration duration)
    {
        deadline_ = now + duration;
        armed_ = true;
    }

    void disarm()
    {
        armed_ = false;
    }

    bool expired(Instant now) const
    {
        return armed_ && now >= deadline_;
    }

private:
    bool armed_ = false;
    Instant deadline_ = Instant(0);
};
