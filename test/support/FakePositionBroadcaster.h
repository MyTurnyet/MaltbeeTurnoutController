#pragma once

#include "ports/PositionBroadcaster.h"

class FakePositionBroadcaster : public PositionBroadcaster
{
public:
    void reportAll() override
    {
        ++reportAllCalls_;
    }

    int reportAllCalls() const
    {
        return reportAllCalls_;
    }

private:
    int reportAllCalls_ = 0;
};
