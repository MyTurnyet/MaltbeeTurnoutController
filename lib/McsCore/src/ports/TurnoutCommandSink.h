#pragma once

#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"

class TurnoutCommandSink
{
public:
    virtual ~TurnoutCommandSink() = default;
    virtual void command(TurnoutId id, TurnoutPosition position) = 0;
};
