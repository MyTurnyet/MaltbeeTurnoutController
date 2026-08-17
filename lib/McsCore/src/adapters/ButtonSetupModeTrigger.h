#pragma once

#include "ports/SetupModeTrigger.h"
#include "ports/DigitalInput.h"
#include "domain/Level.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

class ButtonSetupModeTrigger : public SetupModeTrigger
{
public:
    ButtonSetupModeTrigger(DigitalInput& bootPin, Duration bootWindow)
        : bootPin_(bootPin), bootWindow_(bootWindow)
    {
    }

    // Call repeatedly with the current time during the boot window, before
    // normal operation starts. Non-blocking - no delay().
    void poll(Instant now)
    {
        if (firstSample_)
        {
            windowStart_ = now;
            firstSample_ = false;
        }

        if (bootPin_.read() != Level::Low)
        {
            heldThroughout_ = false;
        }

        elapsedSinceWindowStart_ = now - windowStart_;
    }

    bool requested() const override
    {
        return heldThroughout_ && elapsedSinceWindowStart_ >= bootWindow_;
    }

private:
    DigitalInput& bootPin_;
    Duration bootWindow_;
    bool firstSample_ = true;
    Instant windowStart_ = Instant(0);
    Duration elapsedSinceWindowStart_ = Duration(0);
    bool heldThroughout_ = true;
};
