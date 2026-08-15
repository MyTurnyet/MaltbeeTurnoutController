#pragma once

#include "domain/Instant.h"

class Clock
{
public:
    virtual ~Clock() = default;
    virtual Instant now() const = 0;
};
