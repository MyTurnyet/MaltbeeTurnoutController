#pragma once

#ifdef ARDUINO

#include <optional>
#include <string>

#include "ports/MqttTransport.h"
#include "domain/TopicScheme.h"
#include "domain/PayloadCodec.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutRegistry.h"
#include "ports/TurnoutCommandSink.h"

class MqttCommandSource
{
public:
    MqttCommandSource(MqttTransport& transport, TurnoutCommandSink& sink)
        : transport_(transport), sink_(sink)
    {
    }

    void subscribeAll(int nodeId)
    {
        for (int channel = 1; channel <= TurnoutRegistry::TurnoutsPerNode; ++channel)
        {
            TurnoutId id(nodeId * 100 + channel);
            std::string topic = TopicScheme::topicFor(id);
            transport_.subscribe(topic, [this, id](const std::string& payload) {
                handle(id, payload);
            });
        }
    }

    // Edge-triggered: true if a turnout command was successfully decoded
    // and dispatched since the last call, then clears back to false. Lets
    // src/main.cpp drive an LED flash without polling per-turnout state.
    bool consumeReceived()
    {
        bool wasReceived = receivedThisTick_;
        receivedThisTick_ = false;
        return wasReceived;
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
        receivedThisTick_ = true;
    }

    MqttTransport& transport_;
    TurnoutCommandSink& sink_;
    bool receivedThisTick_ = false;
};

#endif
