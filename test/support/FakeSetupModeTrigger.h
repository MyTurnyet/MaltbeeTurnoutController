#pragma once

#include "ports/SetupModeTrigger.h"

class FakeSetupModeTrigger : public SetupModeTrigger
{
public:
    void setRequested(bool requested)
    {
        requested_ = requested;
    }

    bool requested() const override
    {
        return requested_;
    }

private:
    bool requested_ = false;
};
