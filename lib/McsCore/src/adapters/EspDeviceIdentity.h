// lib/McsCore/src/adapters/EspDeviceIdentity.h
#pragma once

#ifdef ARDUINO

#include <WiFi.h>

#include "ports/DeviceIdentity.h"
#include "domain/MacAddress.h"

class EspDeviceIdentity : public DeviceIdentity
{
public:
    MacAddress mac() const override
    {
        std::array<uint8_t, 6> bytes{};
        WiFi.macAddress(bytes.data());
        return MacAddress(bytes);
    }
};

#endif
