#pragma once

#include "domain/NodeConfig.h"

class ConfigStore
{
public:
    virtual ~ConfigStore() = default;
    virtual void save(const NodeConfig& config) = 0;
    virtual NodeConfig load() = 0;
};
