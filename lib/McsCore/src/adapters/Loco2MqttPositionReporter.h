#pragma once

#include <array>

#include "domain/PayloadCodec.h"
#include "domain/PositionTopic.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutState.h"
#include "ports/MqttTransport.h"
#include "ports/PositionReporter.h"

// Publishes each turnout's state on mcs/turnout/<LocoNet address>/position,
// never retained. Turnouts without a LocoNet address stay silent.
class Loco2MqttPositionReporter : public PositionReporter
{
public:
    Loco2MqttPositionReporter(MqttTransport& transport, int nodeId, std::array<int, 8> locoNetAddresses)
        : transport_(transport), nodeId_(nodeId), locoNetAddresses_(locoNetAddresses)
    {
    }

    void report(TurnoutId id, TurnoutState state) override
    {
        const int address = addressFor(id);
        if (address != 0)
        {
            transport_.publish(PositionTopic(address).value(), PayloadCodec::encode(state), false);
        }
    }

private:
    int addressFor(TurnoutId id) const
    {
        const int channel = id.value() % 100;
        const bool ours = id.value() / 100 == nodeId_ && channel >= 1 && channel <= 8;
        return ours ? locoNetAddresses_[channel - 1] : 0;
    }

    MqttTransport& transport_;
    int nodeId_;
    std::array<int, 8> locoNetAddresses_;
};
