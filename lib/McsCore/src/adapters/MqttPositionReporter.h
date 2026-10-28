#pragma once

#ifdef ARDUINO

#include <string>

#include "ports/MqttTransport.h"
#include "ports/PositionReporter.h"
#include "domain/TopicScheme.h"
#include "domain/PayloadCodec.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutState.h"

class MqttPositionReporter : public PositionReporter
{
public:
    explicit MqttPositionReporter(MqttTransport& transport) : transport_(transport)
    {
    }

    void report(TurnoutId id, TurnoutState state) override
    {
        std::string topic = TopicScheme::topicFor(id);
        std::string payload = PayloadCodec::encode(state);
        transport_.publish(topic, payload, true);
    }

private:
    MqttTransport& transport_;
};

#endif
