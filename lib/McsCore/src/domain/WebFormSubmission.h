#pragma once

#include <array>
#include <string>

struct WebFormTurnoutField
{
    std::string pin;
    std::string feedbackPin;
    std::string orientation;
    std::string settleMs;
    std::string timeoutMs;
};

struct WebFormSubmission
{
    std::string nodeId;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string brokerHost;
    std::string brokerPort;
    std::array<WebFormTurnoutField, 8> turnouts;
};
