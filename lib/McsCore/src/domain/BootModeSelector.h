#pragma once

#include "domain/BootMode.h"
#include "domain/NodeConfig.h"

class BootModeSelector
{
public:
    static BootMode select(const NodeConfig& config, bool wirelessSetupRequested)
    {
        if (wirelessSetupRequested)
        {
            return BootMode::WirelessSetup;
        }

        return config.validate().empty() ? BootMode::Normal : BootMode::NeedsCommissioning;
    }
};
