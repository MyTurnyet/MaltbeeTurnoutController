#pragma once

#include <string>
#include <utility>

class WifiCredentials
{
public:
    WifiCredentials(std::string ssid, std::string password)
        : ssid_(std::move(ssid)), password_(std::move(password))
    {
    }

    const std::string& ssid() const
    {
        return ssid_;
    }

    const std::string& password() const
    {
        return password_;
    }

    bool operator==(const WifiCredentials& other) const
    {
        return ssid_ == other.ssid_ && password_ == other.password_;
    }

    bool operator!=(const WifiCredentials& other) const
    {
        return !(*this == other);
    }

private:
    std::string ssid_;
    std::string password_;
};
