#pragma once

#include "ports/IdentifyRequestTrigger.h"
#include "ports/DigitalInput.h"
#include "domain/Level.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

class ButtonIdentifyRequestTrigger : public IdentifyRequestTrigger
{
public:
    ButtonIdentifyRequestTrigger(DigitalInput& bootPin, Duration minPressDuration, Duration maxPressDuration)
        : bootPin_(bootPin), minPressDuration_(minPressDuration), maxPressDuration_(maxPressDuration)
    {
    }

    // Call repeatedly with the current time. Non-blocking - no delay().
    void poll(Instant now)
    {
        Level level = bootPin_.read();
        requestedThisTick_ = false;

        if (level == Level::Low && !pressed_)
        {
            pressed_ = true;
            pressStart_ = now;
        }
        else if (level == Level::High && pressed_)
        {
            pressed_ = false;
            Duration heldFor = now - pressStart_;
            if (heldFor >= minPressDuration_ && heldFor <= maxPressDuration_)
            {
                requestedThisTick_ = true;
            }
        }
    }

    bool requested() const override
    {
        return requestedThisTick_;
    }

private:
    DigitalInput& bootPin_;
    Duration minPressDuration_;
    Duration maxPressDuration_;
    bool pressed_ = false;
    Instant pressStart_ = Instant(0);
    bool requestedThisTick_ = false;
};
