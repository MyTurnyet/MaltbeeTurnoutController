#pragma once

#ifdef ARDUINO

#include <WiFi.h>

#include <string>

#include "ports/Clock.h"
#include "domain/Instant.h"
#include "domain/Duration.h"
#include "domain/WifiCredentials.h"

class WiFiLink
{
public:
    WiFiLink(Clock& clock, Duration retryInterval)
        : clock_(clock), retryInterval_(retryInterval), lastAttempt_(Instant(0))
    {
    }

    void begin(const WifiCredentials& credentials)
    {
        ssid_ = credentials.ssid();
        password_ = credentials.password();
        connect();
    }

    void poll()
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            return;
        }

        if (clock_.now() - lastAttempt_ >= retryInterval_)
        {
            connect();
        }
    }

    bool connected() const
    {
        return WiFi.status() == WL_CONNECTED;
    }

private:
    void connect()
    {
        WiFi.begin(ssid_.c_str(), password_.c_str());
        lastAttempt_ = clock_.now();
    }

    Clock& clock_;
    Duration retryInterval_;
    Instant lastAttempt_;
    std::string ssid_;
    std::string password_;
};

#endif
