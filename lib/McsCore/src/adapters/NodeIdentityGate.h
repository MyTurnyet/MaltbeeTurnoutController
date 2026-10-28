#pragma once

#include "domain/NodeIdentityGuard.h"
#include "ports/CommandGate.h"

// Closes for good once another board has been seen announcing this node id.
class NodeIdentityGate : public CommandGate
{
public:
    explicit NodeIdentityGate(const NodeIdentityGuard& guard) : guard_(guard)
    {
    }

    bool open() const override
    {
        return !guard_.collisionDetected();
    }

private:
    const NodeIdentityGuard& guard_;
};
