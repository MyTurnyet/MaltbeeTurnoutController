#pragma once

#include <deque>
#include <string>

#include "ports/UartPort.h"

class FakeUartPort : public UartPort
{
public:
    void feed(const std::string& text)
    {
        for (char c : text)
        {
            queue_.push_back(c);
        }
    }

    bool available() override
    {
        return !queue_.empty();
    }

    char read() override
    {
        char c = queue_.front();
        queue_.pop_front();
        return c;
    }

    void write(const std::string& text) override
    {
        written_ += text;
    }

    const std::string& written() const
    {
        return written_;
    }

private:
    std::deque<char> queue_;
    std::string written_;
};
