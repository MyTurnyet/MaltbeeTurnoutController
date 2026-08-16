#pragma once

#ifdef ARDUINO

#include <WiFiClient.h>
#include <PubSubClient.h>

#include <string>
#include <utility>

#include "ports/Clock.h"
#include "domain/Instant.h"
#include "domain/Duration.h"
#include "domain/BrokerAddress.h"

class MqttLink
{
public:
    MqttLink(Clock& clock, Duration retryInterval, std::string clientId, std::string willTopic, std::string willMessage)
        : clock_(clock),
          retryInterval_(retryInterval),
          clientId_(std::move(clientId)),
          willTopic_(std::move(willTopic)),
          willMessage_(std::move(willMessage)),
          client_(wifiClient_),
          lastAttempt_(Instant(0))
    {
    }

    void begin(const BrokerAddress& broker)
    {
        client_.setServer(broker.host().c_str(), broker.port());
        connect();
    }

    void poll()
    {
        if (client_.connected())
        {
            client_.loop();
            return;
        }

        if (clock_.now() - lastAttempt_ >= retryInterval_)
        {
            connect();
        }
    }

    // Not const: PubSubClient::connected() isn't const-qualified.
    bool connected()
    {
        return client_.connected();
    }

    PubSubClient& raw()
    {
        return client_;
    }

private:
    void connect()
    {
        client_.connect(clientId_.c_str(), willTopic_.c_str(), 1, true, willMessage_.c_str());
        lastAttempt_ = clock_.now();
    }

    Clock& clock_;
    Duration retryInterval_;
    std::string clientId_;
    std::string willTopic_;
    std::string willMessage_;
    WiFiClient wifiClient_;
    PubSubClient client_;
    Instant lastAttempt_;
};

#endif
