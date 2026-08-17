#pragma once

#include <vector>

#include "ports/NodePresenceReporter.h"

class FakeNodePresenceReporter : public NodePresenceReporter
{
public:
    void announce(NodeId id) override
    {
        announced_.push_back(id);
    }

    const std::vector<NodeId>& announced() const
    {
        return announced_;
    }

private:
    std::vector<NodeId> announced_;
};
