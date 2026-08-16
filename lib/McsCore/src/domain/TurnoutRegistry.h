#pragma once

#include <array>
#include <optional>
#include <utility>

#include "domain/Turnout.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/Instant.h"
#include "ports/TurnoutCommandSink.h"

class TurnoutRegistry : public TurnoutCommandSink
{
public:
    static constexpr int TurnoutsPerNode = 8;

    TurnoutRegistry(int nodeId, std::array<Turnout, TurnoutsPerNode> turnouts)
        : nodeId_(nodeId), turnouts_(std::move(turnouts))
    {
    }

    void command(TurnoutId id, TurnoutPosition position) override
    {
        int value = id.value();
        int owningNode = value / 100;
        int channel = value % 100;

        if (owningNode != nodeId_)
        {
            return;
        }

        if (channel < 1 || channel > TurnoutsPerNode)
        {
            return;
        }

        pending_[channel - 1] = position;
    }

    void tick(Instant now)
    {
        for (int i = 0; i < TurnoutsPerNode; ++i)
        {
            if (pending_[i].has_value())
            {
                turnouts_[i].moveTo(*pending_[i], now);
                pending_[i].reset();
            }
        }

        for (auto& turnout : turnouts_)
        {
            turnout.tick(now);
        }
    }

private:
    int nodeId_;
    std::array<Turnout, TurnoutsPerNode> turnouts_;
    std::array<std::optional<TurnoutPosition>, TurnoutsPerNode> pending_;
};
