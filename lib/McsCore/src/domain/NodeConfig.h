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
    static constexpr int kMinLocoNetAddress = 1;
    static constexpr int kMaxLocoNetAddress = 2048;

    NodeConfig(NodeId id, WifiCredentials wifi, BrokerAddress broker, std::array<TurnoutConfig, 8> turnouts,
               std::array<int, 8> locoNetAddresses = {})
        : id_(id),
          wifi_(std::move(wifi)),
          broker_(std::move(broker)),
          turnouts_(std::move(turnouts)),
          locoNetAddresses_(locoNetAddresses)
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
        return NodeConfig(id, wifi_, broker_, turnouts_, locoNetAddresses_);
    }

    NodeConfig withWifi(WifiCredentials wifi) const
    {
        return NodeConfig(id_, std::move(wifi), broker_, turnouts_, locoNetAddresses_);
    }

    NodeConfig withBroker(BrokerAddress broker) const
    {
        return NodeConfig(id_, wifi_, std::move(broker), turnouts_, locoNetAddresses_);
    }

    NodeConfig withTurnout(int index, TurnoutConfig turnout) const
    {
        std::array<TurnoutConfig, 8> updated = turnouts_;
        updated[index] = turnout;
        return NodeConfig(id_, wifi_, broker_, updated, locoNetAddresses_);
    }

    // LocoNet turnout address for each channel; 0 means unconfigured.
    const std::array<int, 8>& locoNetAddresses() const
    {
        return locoNetAddresses_;
    }

    NodeConfig withLocoNetAddress(int index, int address) const
    {
        std::array<int, 8> updated = locoNetAddresses_;
        updated[index] = address;
        return NodeConfig(id_, wifi_, broker_, turnouts_, updated);
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

        appendLocoNetAddressErrors(errors);
        return errors;
    }

    bool operator==(const NodeConfig& other) const
    {
        return id_ == other.id_ && wifi_ == other.wifi_ && broker_ == other.broker_ && turnouts_ == other.turnouts_
            && locoNetAddresses_ == other.locoNetAddresses_;
    }

    bool operator!=(const NodeConfig& other) const
    {
        return !(*this == other);
    }

private:
    void appendLocoNetAddressErrors(std::vector<ConfigError>& errors) const
    {
        for (int index = 0; index < 8; ++index)
        {
            appendLocoNetAddressErrorsFor(index, errors);
        }
    }

    void appendLocoNetAddressErrorsFor(int index, std::vector<ConfigError>& errors) const
    {
        const int address = locoNetAddresses_[index];
        if (address == 0)
        {
            return;
        }
        if (address < kMinLocoNetAddress || address > kMaxLocoNetAddress)
        {
            errors.push_back(ConfigError{"Turnout " + std::to_string(index + 1) +
                                         " LocoNet address out of range (must be 0 or 1-2048)"});
            return;
        }
        appendDuplicateLocoNetAddressErrors(index, errors);
    }

    void appendDuplicateLocoNetAddressErrors(int index, std::vector<ConfigError>& errors) const
    {
        for (int other = index + 1; other < 8; ++other)
        {
            if (locoNetAddresses_[other] == locoNetAddresses_[index])
            {
                errors.push_back(ConfigError{"LocoNet address " + std::to_string(locoNetAddresses_[index]) +
                                             " used by turnouts " + std::to_string(index + 1) + " and " +
                                             std::to_string(other + 1)});
            }
        }
    }

    NodeId id_;
    WifiCredentials wifi_;
    BrokerAddress broker_;
    std::array<TurnoutConfig, 8> turnouts_;
    std::array<int, 8> locoNetAddresses_;
};
