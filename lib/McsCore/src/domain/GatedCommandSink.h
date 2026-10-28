#pragma once

#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "ports/CommandGate.h"
#include "ports/TurnoutCommandSink.h"

// Passes commands on only while the gate is open; otherwise drops them.
class GatedCommandSink : public TurnoutCommandSink
{
public:
    GatedCommandSink(TurnoutCommandSink& inner, const CommandGate& gate) : inner_(inner), gate_(gate)
    {
    }

    void command(TurnoutId id, TurnoutPosition position) override
    {
        if (gate_.open())
        {
            inner_.command(id, position);
        }
    }

private:
    TurnoutCommandSink& inner_;
    const CommandGate& gate_;
};
