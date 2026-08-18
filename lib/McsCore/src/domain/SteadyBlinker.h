#pragma once

#include "domain/Duration.h"
#include "domain/Level.h"

// Fixed-rate on/off square wave, visually distinct from BlinkOutIdentifier's
// per-id blink-count pattern. Used for the collision-error indicator, where
// the node's id can't be trusted (that's the whole problem).
class SteadyBlinker
{
public:
    explicit SteadyBlinker(Duration halfPeriod) : halfPeriod_(halfPeriod)
    {
    }

    Level levelAt(Duration elapsed) const
    {
        unsigned long half = halfPeriod_.milliseconds();
        unsigned long t = elapsed.milliseconds() % (half * 2);
        return t < half ? Level::High : Level::Low;
    }

private:
    Duration halfPeriod_;
};
