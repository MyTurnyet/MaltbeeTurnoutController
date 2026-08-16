#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/Clock.h"
#include "domain/Instant.h"

// millis() wraps after ~49 days; per docs/software-class-list.md open item
// 10.6, deliberately unhandled here — a reboot happens well inside that
// window in practice, and Instant/Duration's unsigned-arithmetic comparisons
// already tolerate the single wrap-around case a reboot doesn't cover.
class ArduinoClock : public Clock
{
public:
    Instant now() const override
    {
        return Instant(millis());
    }
};

#endif
