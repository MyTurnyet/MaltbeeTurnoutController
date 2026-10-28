#pragma once

#include <utility>
#include <vector>

#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "ports/TurnoutCommandSink.h"

class FakeTurnoutCommandSink : public TurnoutCommandSink
{
public:
    void command(TurnoutId id, TurnoutPosition position) override
    {
        commands_.emplace_back(id, position);
    }

    const std::vector<std::pair<TurnoutId, TurnoutPosition>>& commands() const
    {
        return commands_;
    }

private:
    std::vector<std::pair<TurnoutId, TurnoutPosition>> commands_;
};
