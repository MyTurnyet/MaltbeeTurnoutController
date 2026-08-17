#pragma once

#include "ports/DeviceIdentity.h"

class FakeDeviceIdentity : public DeviceIdentity
{
public:
    explicit FakeDeviceIdentity(MacAddress mac) : mac_(mac)
    {
    }

    MacAddress mac() const override
    {
        return mac_;
    }

private:
    MacAddress mac_;
};
