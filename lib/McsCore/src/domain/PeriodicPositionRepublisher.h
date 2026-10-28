#pragma once

#include "domain/Duration.h"
#include "domain/Instant.h"
#include "ports/PositionBroadcaster.h"

// Re-sends every turnout's position when the broker connection comes up and
// then once per interval while it stays up, so a listener that missed a
// change (or joined late) catches up without anything being retained.
class PeriodicPositionRepublisher
{
public:
    PeriodicPositionRepublisher(PositionBroadcaster& broadcaster, Duration interval)
        : broadcaster_(broadcaster), interval_(interval), lastRepublished_(Instant(0))
    {
    }

    void update(Instant now, bool connected)
    {
        if (connected && due(now))
        {
            broadcaster_.reportAll();
            lastRepublished_ = now;
        }
        wasConnected_ = connected;
    }

private:
    bool due(Instant now) const
    {
        return !wasConnected_ || now - lastRepublished_ >= interval_;
    }

    PositionBroadcaster& broadcaster_;
    Duration interval_;
    Instant lastRepublished_;
    bool wasConnected_ = false;
};
