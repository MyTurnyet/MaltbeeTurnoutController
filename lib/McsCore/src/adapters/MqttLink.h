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
#include "domain/MqttTopicRouter.h"

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
        client_.setCallback([this](char* topic, byte* payload, unsigned int length) {
            std::string text(reinterpret_cast<char*>(payload), length);
            router_.dispatch(topic, text);
        });
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

    // The one place PubSubClient::setCallback is ever called is this
    // class's constructor above - every subscriber routes through here so
    // no caller can silently clobber another's callback registration.
    void subscribe(const std::string& topic, MqttTopicRouter::Handler handler)
    {
        router_.on(topic, std::move(handler));
        client_.subscribe(topic.c_str());
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
    MqttTopicRouter router_;
};

#endif
