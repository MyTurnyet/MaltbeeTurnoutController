#pragma once

#include "domain/NodeId.h"

enum class PresenceVerdict
{
    Self,
    Collision,
    Unrelated
};

class NodeIdCollisionGuard
{
public:
    explicit NodeIdCollisionGuard(NodeId selfId) : selfId_(selfId)
    {
    }

    PresenceVerdict evaluate(NodeId observedId, bool hasAnnouncedSelf) const
    {
        if (observedId != selfId_)
        {
            return PresenceVerdict::Unrelated;
        }

        return hasAnnouncedSelf ? PresenceVerdict::Self : PresenceVerdict::Collision;
    }

private:
    NodeId selfId_;
};
