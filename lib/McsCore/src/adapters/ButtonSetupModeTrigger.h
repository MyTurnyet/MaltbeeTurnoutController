#pragma once

#include "ports/SetupModeTrigger.h"
#include "ports/DigitalInput.h"
#include "domain/Level.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

// Detects "hold BOOT for at least minHoldDuration, then release it" during
// normal runtime polling - deliberately NOT a boot-time strapping-pin read.
// GPIO0 (BOOT) is the ESP32's own boot-mode strapping pin: holding it low
// through a power-on or reset puts the ROM bootloader into permanent UART
// download mode before any application code runs, so a "held through
// power-on" gesture can never be observed from setup()/loop() at all. This
// must instead be read live, well after boot has already completed
// normally - exactly like this same pin's ButtonIdentifyRequestTrigger
// short-press already does.
class ButtonSetupModeTrigger : public SetupModeTrigger
{
public:
    ButtonSetupModeTrigger(DigitalInput& bootPin, Duration minHoldDuration)
        : bootPin_(bootPin), minHoldDuration_(minHoldDuration)
    {
    }

    // Call repeatedly with the current time during normal operation.
    // Non-blocking - no delay(). Edge-triggered: fires the tick BOOT is
    // released, but only if it had been held continuously for at least
    // minHoldDuration_ first.
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
            if (heldFor >= minHoldDuration_)
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
    Duration minHoldDuration_;
    bool pressed_ = false;
    Instant pressStart_ = Instant(0);
    bool requestedThisTick_ = false;
};
