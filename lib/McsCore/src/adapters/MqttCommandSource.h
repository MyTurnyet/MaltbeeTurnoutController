#pragma once

#ifdef ARDUINO

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
    }

    void subscribeAll(int nodeId)
    {
        for (int channel = 1; channel <= TurnoutRegistry::TurnoutsPerNode; ++channel)
        {
            TurnoutId id(nodeId * 100 + channel);
            std::string topic = TopicScheme::topicFor(id);
            link_.subscribe(topic, [this, id](const std::string& payload) {
                handle(id, payload);
            });
        }
    }

private:
    void handle(TurnoutId id, const std::string& payload)
    {
        std::optional<TurnoutPosition> position = PayloadCodec::decode(payload);
        if (!position.has_value())
        {
            return;
        }

        sink_.command(id, *position);
    }

    MqttLink& link_;
    TurnoutCommandSink& sink_;
};

#endif
