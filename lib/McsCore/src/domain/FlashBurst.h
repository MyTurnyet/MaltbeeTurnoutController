#pragma once

#include "domain/Duration.h"
#include "domain/Level.h"

// One-shot burst of flashCount on/off blinks, then stays dark forever -
// unlike BlinkOutIdentifier, there is no pause-and-repeat. A fresh trigger
// (an external Deadline re-arm, driven by the composition root) is what
// restarts it, not levelAt() looping on its own.
class FlashBurst
{
public:
    FlashBurst(Duration onDuration, Duration offDuration, int flashCount)
        : onDuration_(onDuration), offDuration_(offDuration), flashCount_(flashCount)
    {
    }

    Level levelAt(Duration elapsed) const
    {
        unsigned long cycleMs = onDuration_.milliseconds() + offDuration_.milliseconds();
        unsigned long totalMs = cycleMs * static_cast<unsigned long>(flashCount_);
        unsigned long t = elapsed.milliseconds();

        if (t >= totalMs)
        {
            return Level::Low;
        }

        unsigned long withinCycle = t % cycleMs;
        return withinCycle < onDuration_.milliseconds() ? Level::High : Level::Low;
    }

private:
    Duration onDuration_;
    Duration offDuration_;
    int flashCount_;
};
