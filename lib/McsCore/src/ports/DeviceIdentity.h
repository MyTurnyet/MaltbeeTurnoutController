#pragma once

#include "domain/MacAddress.h"

class DeviceIdentity
{
public:
    virtual ~DeviceIdentity() = default;
    virtual MacAddress mac() const = 0;
};
