#pragma once

class PositionBroadcaster
{
public:
    virtual ~PositionBroadcaster() = default;
    virtual void reportAll() = 0;
};
