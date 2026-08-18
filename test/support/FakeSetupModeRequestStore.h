#pragma once

#include "ports/SetupModeRequestStore.h"

class FakeSetupModeRequestStore : public SetupModeRequestStore
{
public:
    void requestOnNextBoot() override
    {
        pending_ = true;
    }

    bool consumeRequest() override
    {
        bool wasPending = pending_;
        pending_ = false;
        return wasPending;
    }

private:
    bool pending_ = false;
};
