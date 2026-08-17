#pragma once

#include <string>

#include "domain/MacAddress.h"

class SetupApName
{
public:
    static std::string from(const MacAddress& mac)
    {
        return "Tortoise-Setup-" + mac.lastFourHexDigits();
    }
};
