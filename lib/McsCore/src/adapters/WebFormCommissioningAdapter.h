#pragma once

#include <string>
#include <vector>

#include "domain/WebFormSubmission.h"
#include "domain/CommissioningSession.h"
#include "domain/CommandLineParser.h"
#include "domain/ParsedCommand.h"

class WebFormCommissioningAdapter
{
public:
    explicit WebFormCommissioningAdapter(CommissioningSession& session) : session_(session)
    {
    }

    std::string submit(const WebFormSubmission& form)
    {
        std::vector<std::string> lines = buildCommandLines(form);

        for (const std::string& line : lines)
        {
            CommissioningResult result = session_.apply(CommandLineParser::parse(line));
            if (isError(result.response))
            {
                return result.response;
            }
        }

        CommissioningResult saveResult = session_.apply(ParsedCommand::save());
        if (isError(saveResult.response))
        {
            return saveResult.response;
        }

        CommissioningResult rebootResult = session_.apply(ParsedCommand::reboot());
        if (rebootResult.rebootRequested)
        {
            rebootRequested_ = true;
        }
        return rebootResult.response;
    }

    bool rebootRequested() const
    {
        return rebootRequested_;
    }

private:
    static bool isError(const std::string& response)
    {
        return response.rfind("ERROR", 0) == 0;
    }

    static std::vector<std::string> buildCommandLines(const WebFormSubmission& form)
    {
        std::vector<std::string> lines;
        lines.push_back("id " + form.nodeId);
        lines.push_back("wifi \"" + form.wifiSsid + "\" \"" + form.wifiPassword + "\"");
        lines.push_back("broker " + form.brokerHost + " " + form.brokerPort);

        for (size_t i = 0; i < form.turnouts.size(); ++i)
        {
            const WebFormTurnoutField& field = form.turnouts[i];
            if (field.pin.empty())
            {
                continue;
            }

            lines.push_back("turnout " + std::to_string(i + 1)
                + " pin " + field.pin
                + " fb " + field.feedbackPin
                + " orientation " + field.orientation
                + " settle " + field.settleMs
                + " timeout " + field.timeoutMs);
        }

        return lines;
    }

    CommissioningSession& session_;
    bool rebootRequested_ = false;
};
