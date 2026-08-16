#pragma once

#ifdef ARDUINO

#include <string>

#include "adapters/MqttLink.h"
#include "ports/PositionReporter.h"
#include "domain/TopicScheme.h"
#include "domain/PayloadCodec.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutState.h"

class MqttPositionReporter : public PositionReporter
{
public:
    explicit MqttPositionReporter(MqttLink& link) : link_(link)
    {
    }

    void report(TurnoutId id, TurnoutState state) override
    {
        std::string topic = TopicScheme::topicFor(id);
        std::string payload = PayloadCodec::encode(state);
        link_.raw().publish(topic.c_str(), payload.c_str(), true);
    }

private:
    MqttLink& link_;
};

#endif
