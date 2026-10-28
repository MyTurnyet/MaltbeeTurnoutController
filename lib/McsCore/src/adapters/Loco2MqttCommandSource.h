#pragma once

#include <array>
#include <optional>

#include "adapters/Loco2MqttStateSource.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "ports/MqttTransport.h"
#include "ports/TurnoutCommandSink.h"

// Turns the states Loco2MQTT publishes for this board's LocoNet addresses
// into commands for the board's own turnouts. Nothing is subscribed until
// listen() is called, so a blocked node can stay silent.
class Loco2MqttCommandSource
{
public:
    Loco2MqttCommandSource(MqttTransport& transport, TurnoutCommandSink& sink, int nodeId,
                           std::array<int, 8> locoNetAddresses)
        : transport_(transport), sink_(sink), nodeId_(nodeId), locoNetAddresses_(locoNetAddresses)
    {
    }

    void listen()
    {
        if (!states_.has_value())
        {
            states_.emplace(transport_, locoNetAddresses_);
        }
    }

    void poll()
    {
        Loco2MqttStateUpdate update{};
        while (states_.has_value() && states_->poll(update))
        {
            dispatch(update);
        }
    }

    // Edge-triggered: true if a command was dispatched since the last call.
    bool consumeReceived()
    {
        const bool wasReceived = received_;
        received_ = false;
        return wasReceived;
    }

private:
    void dispatch(const Loco2MqttStateUpdate& update)
    {
        sink_.command(TurnoutId(nodeId_ * 100 + update.channel), positionFor(update.position));
        received_ = true;
    }

    TurnoutPosition positionFor(Loco2MqttPosition position) const
    {
        return position == Loco2MqttPosition::Closed ? TurnoutPosition::closed() : TurnoutPosition::thrown();
    }

    MqttTransport& transport_;
    TurnoutCommandSink& sink_;
    int nodeId_;
    std::array<int, 8> locoNetAddresses_;
    std::optional<Loco2MqttStateSource8> states_;
    bool received_ = false;
};
