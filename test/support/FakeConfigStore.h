#pragma once

#include <optional>

#include "ports/ConfigStore.h"
#include "domain/NodeConfig.h"

class FakeConfigStore : public ConfigStore
{
public:
    void save(const NodeConfig& config) override
    {
        saved_ = config;
    }

    NodeConfig load() override
    {
        if (saved_.has_value())
        {
            return *saved_;
        }

        return NodeConfig::factoryDefault();
    }

private:
    std::optional<NodeConfig> saved_;
};
