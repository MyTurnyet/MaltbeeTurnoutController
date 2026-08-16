#pragma once

#include <string>
#include <utility>

class BrokerAddress
{
public:
    BrokerAddress(std::string host, int port)
        : host_(std::move(host)), port_(port)
    {
    }

    const std::string& host() const
    {
        return host_;
    }

    int port() const
    {
        return port_;
    }

    bool operator==(const BrokerAddress& other) const
    {
        return host_ == other.host_ && port_ == other.port_;
    }

    bool operator!=(const BrokerAddress& other) const
    {
        return !(*this == other);
    }

private:
    std::string host_;
    int port_;
};
