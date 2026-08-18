#pragma once

#include "domain/BootMode.h"
#include "domain/NodeConfig.h"

class BootModeSelector
{
public:
    static BootMode select(const NodeConfig& config)
    {
        return config.validate().empty() ? BootMode::Normal : BootMode::NeedsCommissioning;
    }
};
