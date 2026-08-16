#pragma once

#include <exception>
#include <sstream>
#include <string>
#include <vector>

#include "domain/ParsedCommand.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutConfig.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

class CommandLineParser
{
public:
    static ParsedCommand parse(const std::string& line)
    {
        std::vector<std::string> tokens = tokenize(line);

        if (tokens.empty())
        {
            return ParsedCommand::invalid("empty command");
        }

        const std::string& keyword = tokens[0];

        if (keyword == "id" && tokens.size() == 2)
        {
            return parseSetId(tokens);
        }
        if (keyword == "wifi" && tokens.size() == 3)
        {
            return ParsedCommand::setWifi(tokens[1], tokens[2]);
        }
        if (keyword == "broker" && tokens.size() == 3)
        {
            return parseSetBroker(tokens);
        }
        if (keyword == "turnout" && tokens.size() == 12)
        {
            return parseSetTurnout(tokens);
        }
        if (keyword == "show" && tokens.size() == 1)
        {
            return ParsedCommand::show();
        }
        if (keyword == "save" && tokens.size() == 1)
        {
            return ParsedCommand::save();
        }
        if (keyword == "reboot" && tokens.size() == 1)
        {
            return ParsedCommand::reboot();
        }

        return ParsedCommand::invalid("unrecognized command: " + line);
    }

private:
    static std::vector<std::string> tokenize(const std::string& line)
    {
        std::vector<std::string> tokens;
        std::istringstream stream(line);
        std::string token;
        while (stream >> token)
        {
            tokens.push_back(token);
        }
        return tokens;
    }

    static bool parseInt(const std::string& text, int& value)
    {
        try
        {
            size_t consumed = 0;
            value = std::stoi(text, &consumed);
            return consumed == text.size();
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    static ParsedCommand parseSetId(const std::vector<std::string>& tokens)
    {
        int value;
        if (!parseInt(tokens[1], value))
        {
            return ParsedCommand::invalid("id: expected an integer");
        }
        return ParsedCommand::setId(value);
    }

    static ParsedCommand parseSetBroker(const std::vector<std::string>& tokens)
    {
        int port;
        if (!parseInt(tokens[2], port))
        {
            return ParsedCommand::invalid("broker: expected an integer port");
        }
        return ParsedCommand::setBroker(tokens[1], port);
    }

    static ParsedCommand parseSetTurnout(const std::vector<std::string>& tokens)
    {
        // turnout <n> pin <gpio> fb <gpio> orientation <normal|inverted> settle <ms> timeout <ms>
        //    0     1   2    3    4   5         6            7             8    9      10     11
        if (tokens[2] != "pin" || tokens[4] != "fb" || tokens[6] != "orientation"
            || tokens[8] != "settle" || tokens[10] != "timeout")
        {
            return ParsedCommand::invalid("turnout: malformed command");
        }

        int n;
        int outputPin;
        int feedbackPin;
        int settleMs;
        int timeoutMs;
        if (!parseInt(tokens[1], n) || !parseInt(tokens[3], outputPin) || !parseInt(tokens[5], feedbackPin)
            || !parseInt(tokens[9], settleMs) || !parseInt(tokens[11], timeoutMs))
        {
            return ParsedCommand::invalid("turnout: expected integers for n/pin/fb/settle/timeout");
        }

        if (n < 1 || n > 8)
        {
            return ParsedCommand::invalid("turnout: n must be 1-8");
        }

        Orientation orientation = Orientation::normal();
        if (tokens[7] == "inverted")
        {
            orientation = Orientation::inverted();
        }
        else if (tokens[7] != "normal")
        {
            return ParsedCommand::invalid("turnout: orientation must be normal or inverted");
        }

        TurnoutConfig config(TurnoutId(n), outputPin, feedbackPin, orientation, Duration(settleMs), Duration(timeoutMs));
        return ParsedCommand::setTurnout(n - 1, config);
    }
};
