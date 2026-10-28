#pragma once

#ifdef ARDUINO

#include <string>

#include "ports/MqttTransport.h"
#include "ports/NodePresenceReporter.h"
#include "domain/NodeId.h"

class MqttNodePresenceReporter : public NodePresenceReporter
{
public:
    explicit MqttNodePresenceReporter(MqttTransport& transport) : transport_(transport)
    {
    }

    void announce(NodeId id) override
    {
        std::string topic = "node/" + std::to_string(id.value()) + "/status";
        transport_.publish(topic, "online", true);
    }

private:
    MqttTransport& transport_;
};

#endif
