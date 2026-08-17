#pragma once

#ifdef ARDUINO

#include <string>

#include "adapters/MqttLink.h"
#include "ports/NodePresenceReporter.h"
#include "domain/NodeId.h"

class MqttNodePresenceReporter : public NodePresenceReporter
{
public:
    explicit MqttNodePresenceReporter(MqttLink& link) : link_(link)
    {
    }

    void announce(NodeId id) override
    {
        std::string topic = "node/" + std::to_string(id.value()) + "/status";
        link_.raw().publish(topic.c_str(), "online", true);
    }

private:
    MqttLink& link_;
};

#endif
