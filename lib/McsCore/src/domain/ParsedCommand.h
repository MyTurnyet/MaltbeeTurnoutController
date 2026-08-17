#pragma once

#include <optional>
#include <string>
#include <utility>

#include "domain/TurnoutConfig.h"

enum class CommandType
{
    SetId,
    SetWifi,
    SetBroker,
    SetTurnout,
    Show,
    Save,
    Reboot,
    Invalid
};

class ParsedCommand
{
public:
    static ParsedCommand setId(int nodeId)
    {
        ParsedCommand command(CommandType::SetId);
        command.nodeId_ = nodeId;
        return command;
    }

    static ParsedCommand setWifi(std::string ssid, std::string password)
    {
        ParsedCommand command(CommandType::SetWifi);
        command.wifiSsid_ = std::move(ssid);
        command.wifiPassword_ = std::move(password);
        return command;
    }

    static ParsedCommand setBroker(std::string host, int port)
    {
        ParsedCommand command(CommandType::SetBroker);
        command.brokerHost_ = std::move(host);
        command.brokerPort_ = port;
        return command;
    }

    static ParsedCommand setTurnout(int index, TurnoutConfig config)
    {
        ParsedCommand command(CommandType::SetTurnout);
        command.turnoutIndex_ = index;
        command.turnoutConfig_ = std::move(config);
        return command;
    }

    static ParsedCommand show()
    {
        return ParsedCommand(CommandType::Show);
    }

    static ParsedCommand save()
    {
        return ParsedCommand(CommandType::Save);
    }

    static ParsedCommand reboot()
    {
        return ParsedCommand(CommandType::Reboot);
    }

    static ParsedCommand invalid(std::string reason)
    {
        ParsedCommand command(CommandType::Invalid);
        command.invalidReason_ = std::move(reason);
        return command;
    }

    CommandType type() const
    {
        return type_;
    }

    int nodeId() const
    {
        return nodeId_;
    }

    const std::string& wifiSsid() const
    {
        return wifiSsid_;
    }

    const std::string& wifiPassword() const
    {
        return wifiPassword_;
    }

    const std::string& brokerHost() const
    {
        return brokerHost_;
    }

    int brokerPort() const
    {
        return brokerPort_;
    }

    int turnoutIndex() const
    {
        return turnoutIndex_;
    }

    const TurnoutConfig& turnoutConfig() const
    {
        return turnoutConfig_.value();
    }

    const std::string& invalidReason() const
    {
        return invalidReason_;
    }

private:
    explicit ParsedCommand(CommandType type) : type_(type)
    {
    }

    CommandType type_;
    int nodeId_ = 0;
    std::string wifiSsid_;
    std::string wifiPassword_;
    std::string brokerHost_;
    int brokerPort_ = 0;
    int turnoutIndex_ = 0;
    std::optional<TurnoutConfig> turnoutConfig_;
    std::string invalidReason_;
};
