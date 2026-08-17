#pragma once

#include <string>

#include "ports/UartPort.h"
#include "domain/CommissioningSession.h"
#include "domain/CommandLineParser.h"
#include "domain/ParsedCommand.h"

class SerialCommissioningAdapter
{
public:
    SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session)
        : uart_(uart), session_(session)
    {
    }

    void poll()
    {
        while (uart_.available())
        {
            char c = uart_.read();

            if (c == '\n')
            {
                handleLine(buffer_);
                buffer_.clear();

                if (rebootRequested_)
                {
                    return;
                }
            }
            else if (c != '\r')
            {
                buffer_.push_back(c);
            }
        }
    }

    bool rebootRequested() const
    {
        return rebootRequested_;
    }

private:
    void handleLine(const std::string& line)
    {
        if (line.empty())
        {
            return;
        }

        ParsedCommand command = CommandLineParser::parse(line);
        CommissioningResult result = session_.apply(command);

        uart_.write(result.response + "\n");

        if (result.rebootRequested)
        {
            rebootRequested_ = true;
        }
    }

    UartPort& uart_;
    CommissioningSession& session_;
    std::string buffer_;
    bool rebootRequested_ = false;
};
