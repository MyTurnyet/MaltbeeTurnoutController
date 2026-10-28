#pragma once

#include <string>

// Where this board publishes the position of the turnout with a given LocoNet address.
class PositionTopic
{
public:
    explicit PositionTopic(int locoNetAddress) : locoNetAddress_(locoNetAddress)
    {
    }

    std::string value() const
    {
        return "mcs/turnout/" + std::to_string(locoNetAddress_) + "/position";
    }

private:
    int locoNetAddress_;
};
