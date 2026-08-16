#pragma once

#include <string>
#include <vector>

#include "domain/ParsedCommand.h"
#include "domain/NodeConfig.h"
#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutConfig.h"
#include "domain/Orientation.h"
#include "domain/Level.h"
#include "domain/TurnoutPosition.h"
#include "ports/ConfigStore.h"

struct CommissioningResult
{
    std::string response;
    bool rebootRequested;
};

class CommissioningSession
{
public:
    explicit CommissioningSession(ConfigStore& store)
        : store_(store), draft_(store.load())
    {
    }

    const NodeConfig& draft() const
    {
        return draft_;
    }

    CommissioningResult apply(const ParsedCommand& command)
    {
        switch (command.type())
        {
        case CommandType::SetId:
            draft_ = draft_.withId(NodeId(command.nodeId()));
            return {"OK", false};

        case CommandType::SetWifi:
            draft_ = draft_.withWifi(WifiCredentials(command.wifiSsid(), command.wifiPassword()));
            return {"OK", false};

        case CommandType::SetBroker:
            draft_ = draft_.withBroker(BrokerAddress(command.brokerHost(), command.brokerPort()));
            return {"OK", false};

        case CommandType::SetTurnout:
            draft_ = draft_.withTurnout(command.turnoutIndex(), command.turnoutConfig());
            return {"OK", false};

        case CommandType::Show:
            return {formatShow(), false};

        case CommandType::Save:
            return applySave();

        case CommandType::Reboot:
            return {"REBOOTING", true};

        case CommandType::Invalid:
            return {"ERROR: " + command.invalidReason(), false};
        }

        return {"ERROR: unhandled command", false};
    }

private:
    CommissioningResult applySave()
    {
        std::vector<ConfigError> errors = draft_.validate();
        if (!errors.empty())
        {
            std::string response = "ERROR: ";
            for (size_t i = 0; i < errors.size(); ++i)
            {
                if (i > 0)
                {
                    response += "; ";
                }
                response += errors[i].message;
            }
            return {response, false};
        }

        store_.save(draft_);
        return {"OK: saved", false};
    }

    std::string formatShow() const
    {
        std::string text;
        text += "id: " + std::to_string(draft_.id().value()) + "\n";
        text += "wifi: " + draft_.wifi().ssid() + "\n";
        text += "broker: " + draft_.broker().host() + ":" + std::to_string(draft_.broker().port()) + "\n";

        for (int i = 0; i < 8; ++i)
        {
            const TurnoutConfig& turnout = draft_.turnouts()[i];
            text += "turnout " + std::to_string(i + 1) + ": pin=" + std::to_string(turnout.outputPin())
                + " fb=" + std::to_string(turnout.feedbackPin())
                + " orientation=" + (isInverted(turnout.orientation()) ? "inverted" : "normal")
                + " settle=" + std::to_string(turnout.settleDuration().milliseconds())
                + " timeout=" + std::to_string(turnout.movementTimeout().milliseconds())
                + "\n";
        }

        return text;
    }

    static bool isInverted(Orientation orientation)
    {
        return orientation.toLevel(TurnoutPosition::closed()) == Level::High;
    }

    ConfigStore& store_;
    NodeConfig draft_;
};
