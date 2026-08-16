#pragma once

#include <optional>
#include <string>

#include "domain/TurnoutPosition.h"
#include "domain/TurnoutState.h"

class PayloadCodec
{
public:
    static std::string encode(TurnoutPosition position)
    {
        return (position == TurnoutPosition::closed()) ? "CLOSED" : "THROWN";
    }

    static std::optional<TurnoutPosition> decode(const std::string& payload)
    {
        if (payload == "CLOSED")
        {
            return TurnoutPosition::closed();
        }

        if (payload == "THROWN")
        {
            return TurnoutPosition::thrown();
        }

        return std::nullopt;
    }

    static std::string encode(TurnoutState state)
    {
        switch (state)
        {
        case TurnoutState::Closed:
            return "CLOSED";
        case TurnoutState::Thrown:
            return "THROWN";
        case TurnoutState::Moving:
            return "INCONSISTENT";
        case TurnoutState::Unknown:
            return "UNKNOWN";
        }

        return "UNKNOWN";
    }
};
