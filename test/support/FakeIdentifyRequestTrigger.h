#pragma once

#include "ports/IdentifyRequestTrigger.h"

class FakeIdentifyRequestTrigger : public IdentifyRequestTrigger
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
