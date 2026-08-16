#pragma once

#include "domain/TurnoutId.h"
#include "domain/TurnoutState.h"

class PositionReporter
{
public:
    virtual ~PositionReporter() = default;
    virtual void report(TurnoutId id, TurnoutState state) = 0;
};
