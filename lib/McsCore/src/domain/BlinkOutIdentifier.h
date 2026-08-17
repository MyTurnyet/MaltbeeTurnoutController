#pragma once

#include "domain/NodeId.h"
#include "domain/Duration.h"
#include "domain/Level.h"

class BlinkOutIdentifier
{
public:
    BlinkOutIdentifier(NodeId id, Duration onDuration, Duration offDuration, Duration pauseDuration)
        : id_(id), onDuration_(onDuration), offDuration_(offDuration), pauseDuration_(pauseDuration)
    {
    }

    Level levelAt(Duration elapsed) const
    {
        unsigned long blinkPeriodMs = onDuration_.milliseconds() + offDuration_.milliseconds();
        unsigned long blinkingPhaseMs = blinkPeriodMs * static_cast<unsigned long>(id_.value());
        unsigned long cycleMs = blinkingPhaseMs + pauseDuration_.milliseconds();

        unsigned long t = elapsed.milliseconds() % cycleMs;

        if (t >= blinkingPhaseMs)
        {
            return Level::Low;
        }

        unsigned long withinBlink = t % blinkPeriodMs;
        return withinBlink < onDuration_.milliseconds() ? Level::High : Level::Low;
    }

private:
    NodeId id_;
    Duration onDuration_;
    Duration offDuration_;
    Duration pauseDuration_;
};
