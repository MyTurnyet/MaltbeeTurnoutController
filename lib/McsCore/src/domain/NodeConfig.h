#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutConfig.h"
#include "domain/TurnoutId.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

struct ConfigError
{
    std::string message;

    bool operator==(const ConfigError& other) const
    {
        return message == other.message;
    }

    bool operator!=(const ConfigError& other) const
    {
        return !(*this == other);
    }
};

class NodeConfig
{
public:
    NodeConfig(NodeId id, WifiCredentials wifi, BrokerAddress broker, std::array<TurnoutConfig, 8> turnouts)
        : id_(id), wifi_(std::move(wifi)), broker_(std::move(broker)), turnouts_(std::move(turnouts))
    {
    }

    static NodeConfig factoryDefault()
    {
        Orientation orientation = Orientation::normal();
        Duration settle(50);
        Duration timeout(200);

        std::array<TurnoutConfig, 8> turnouts{
            TurnoutConfig(TurnoutId(1), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(2), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(3), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(4), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(5), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(6), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(7), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(8), -1, -1, orientation, settle, timeout)
        };

        return NodeConfig(NodeId(0), WifiCredentials("", ""), BrokerAddress("", 1883), turnouts);
    }

    NodeId id() const
    {
        return id_;
    }

    const WifiCredentials& wifi() const
    {
        return wifi_;
    }

    const BrokerAddress& broker() const
    {
        return broker_;
    }

    const std::array<TurnoutConfig, 8>& turnouts() const
    {
        return turnouts_;
    }

    NodeConfig withId(NodeId id) const
    {
        return NodeConfig(id, wifi_, broker_, turnouts_);
    }

    NodeConfig withWifi(WifiCredentials wifi) const
    {
        return NodeConfig(id_, std::move(wifi), broker_, turnouts_);
    }

    NodeConfig withBroker(BrokerAddress broker) const
    {
        return NodeConfig(id_, wifi_, std::move(broker), turnouts_);
    }

    NodeConfig withTurnout(int index, TurnoutConfig turnout) const
    {
        std::array<TurnoutConfig, 8> updated = turnouts_;
        updated[index] = turnout;
        return NodeConfig(id_, wifi_, broker_, updated);
    }

    std::vector<ConfigError> validate() const
    {
        std::vector<ConfigError> errors;

        if (id_.value() < 1 || id_.value() > 16)
        {
            errors.push_back(ConfigError{"Node id out of range (must be 1-16)"});
        }

        std::vector<int> seenPins;
        for (const auto& turnout : turnouts_)
        {
            for (int pin : {turnout.outputPin(), turnout.feedbackPin()})
            {
                if (pin == -1)
                {
                    continue;
                }

                bool alreadySeen = std::find(seenPins.begin(), seenPins.end(), pin) != seenPins.end();
                if (alreadySeen)
                {
                    errors.push_back(ConfigError{"Pin " + std::to_string(pin) + " used by more than one turnout"});
                }
                seenPins.push_back(pin);
            }
        }

        return errors;
    }

    bool operator==(const NodeConfig& other) const
    {
        return id_ == other.id_ && wifi_ == other.wifi_ && broker_ == other.broker_ && turnouts_ == other.turnouts_;
    }

    bool operator!=(const NodeConfig& other) const
    {
        return !(*this == other);
    }

private:
    NodeId id_;
    WifiCredentials wifi_;
    BrokerAddress broker_;
    std::array<TurnoutConfig, 8> turnouts_;
};
