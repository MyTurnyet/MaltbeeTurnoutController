#pragma once

#include <cstdint>

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
        if (!armed_)
        {
            return false;
        }

        // Not `now >= deadline_`: Instant wraps every ~49.7 days (millis()
        // is an unsigned 32-bit counter), and after a wrap `now` can be
        // numerically far smaller than `deadline_` while still being
        // chronologically later. Signed-difference comparison is the
        // standard fix - `now - deadline_` wraps the same way `deadline_`
        // itself was computed, so reinterpreting the unsigned result as
        // signed correctly reports "at or past deadline" across a single
        // wrap, as long as the deadline is never more than ~24.8 days
        // (2^31 ms) out - true for every duration this firmware arms.
        int32_t elapsedSinceDeadline = static_cast<int32_t>((now - deadline_).milliseconds());
        return elapsedSinceDeadline >= 0;
    }

    bool armed() const
    {
        return armed_;
    }

private:
    bool armed_ = false;
    Instant deadline_ = Instant(0);
};
