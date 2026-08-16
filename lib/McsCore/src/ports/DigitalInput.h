#pragma once

#include "domain/Level.h"

class DigitalInput
{
public:
    virtual ~DigitalInput() = default;
    virtual Level read() = 0;
};
