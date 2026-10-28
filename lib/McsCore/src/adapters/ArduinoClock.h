#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "domain/Instant.h"
#include "ports/Clock.h"

// millis() wraps after ~49 days; per docs/software-class-list.md open item
// 10.6, deliberately unhandled here — a reboot happens well inside that
// window in practice, and Instant/Duration's unsigned-arithmetic comparisons
// already tolerate the single wrap-around case a reboot doesn't cover.
//
// Implements the shared mcs-core Clock port (used by the mcs-net links) and
// also offers now() for this project's own Instant-based code.
class ArduinoClock final : public Clock
{
public:
    unsigned long nowMilliseconds() const override
    {
        return millis();
    }

    Instant now() const
    {
        return Instant(nowMilliseconds());
    }
};

#endif
