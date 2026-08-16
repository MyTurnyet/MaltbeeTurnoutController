#pragma once

#include "domain/Level.h"
#include "domain/TurnoutPosition.h"

class Orientation
{
public:
    static Orientation normal()
    {
        return Orientation(false);
    }

    static Orientation inverted()
    {
        return Orientation(true);
    }

    Level toLevel(TurnoutPosition position) const
    {
        bool isThrown = (position != TurnoutPosition::closed());
        bool isHigh = inverted_ ? !isThrown : isThrown;
        return isHigh ? Level::High : Level::Low;
    }

    TurnoutPosition toPosition(Level level) const
    {
        bool isHigh = (level == Level::High);
        bool isThrown = inverted_ ? !isHigh : isHigh;
        return isThrown ? TurnoutPosition::thrown() : TurnoutPosition::closed();
    }

private:
    explicit Orientation(bool inverted) : inverted_(inverted)
    {
    }

    bool inverted_;
};
