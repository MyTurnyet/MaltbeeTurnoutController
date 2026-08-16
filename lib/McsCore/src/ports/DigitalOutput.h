#pragma once

#include "domain/Level.h"

class DigitalOutput
{
public:
    virtual ~DigitalOutput() = default;
    virtual void write(Level level) = 0;
};
