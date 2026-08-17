#pragma once

#include "domain/NodeId.h"

class NodePresenceReporter
{
public:
    virtual ~NodePresenceReporter() = default;
    virtual void announce(NodeId id) = 0;
};
