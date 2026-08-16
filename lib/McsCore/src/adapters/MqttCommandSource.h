#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <PubSubClient.h>

#include <optional>
#include <string>

#include "adapters/MqttLink.h"
#include "domain/TopicScheme.h"
#include "domain/PayloadCodec.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutRegistry.h"
#include "ports/TurnoutCommandSink.h"

class MqttCommandSource
{
public:
    MqttCommandSource(MqttLink& link, TurnoutCommandSink& sink)
        : link_(link), sink_(sink)
    {
        link_.raw().setCallback([this](char* topic, byte* payload, unsigned int length) {
            handle(topic, payload, length);
        });
    }

    void subscribeAll(int nodeId)
    {
        for (int channel = 1; channel <= TurnoutRegistry::TurnoutsPerNode; ++channel)
        {
            std::string topic = TopicScheme::topicFor(TurnoutId(nodeId * 100 + channel));
            link_.raw().subscribe(topic.c_str());
        }
    }

private:
    void handle(char* topic, byte* payload, unsigned int length)
    {
        std::optional<TurnoutId> id = TopicScheme::parse(topic);
        if (!id.has_value())
        {
            return;
        }

        std::string text(reinterpret_cast<char*>(payload), length);
        std::optional<TurnoutPosition> position = PayloadCodec::decode(text);
        if (!position.has_value())
        {
            return;
        }

        sink_.command(*id, *position);
    }

    MqttLink& link_;
    TurnoutCommandSink& sink_;
};

#endif
