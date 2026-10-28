#pragma once

#ifdef ARDUINO

#include <array>
#include <string>

#include "domain/NodeConfig.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/Duration.h"
#include "domain/FeedbackSensor.h"
#include "domain/TurnoutMotion.h"
#include "domain/Turnout.h"
#include "domain/TurnoutRegistry.h"
#include "adapters/ArduinoClock.h"
#include "adapters/EspDigitalOutput.h"
#include "adapters/EspDigitalInput.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/WiFiLink.h"
#include "adapters/MqttLink.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspMdnsResolver.h"
#include "application/BrokerAddressResolver.h"
#include "adapters/Loco2MqttPositionReporter.h"
#include "domain/PeriodicPositionRepublisher.h"
#include "adapters/Loco2MqttCommandSource.h"
#include "adapters/NodeIdentityGate.h"
#include "application/NodePresenceAnnouncer.h"
#include "domain/GatedCommandSink.h"
#include "domain/NodeIdentityGuard.h"
#include "domain/NodePresenceTopics.h"

class ControllerNode
{
public:
    ControllerNode()
    {
    }

    void begin()
    {
        wifiLink_.begin(config_.wifi().ssid(), config_.wifi().password());
        mqttLink_.begin(config_.broker().host(), config_.broker().port());

        mqttLink_.subscribe(presenceTopics_.macTopic(), [this](const std::string& mac) {
            identityGuard_.onMacObserved(mac);
        });
        commandSource_.listen();
    }

    void tick()
    {
        wifiLink_.poll();
        mqttLink_.poll();
        announcer_.update(mqttLink_.connected());
        commandSource_.poll();
        republisher_.update(clock_.now(), mqttLink_.connected());
        registry_.tick(clock_.now());
    }

    // Forces every turnout to republish its current status over MQTT,
    // regardless of whether it has changed since the last publish. Used by
    // the identify short-press so JMRI (or a human checking mosquitto_sub)
    // can get a full resync on demand.
    void republishAllStatus()
    {
        registry_.reportAll();
    }

    // True once another board has been seen announcing this node's id (its
    // MAC differs from ours on the shared presence topic). Turnout commands
    // are dropped from then on - src/main.cpp drives a distinct error blink
    // pattern instead of the normal identify blink.
    bool blocked() const
    {
        return identityGuard_.collisionDetected();
    }


    // True if a turnout command was successfully decoded and dispatched
    // since the last call to this method (edge-triggered, read-and-clear).
    // src/main.cpp calls this once per loop() tick, right after tick(), to
    // drive the MQTT-activity flash-burst on the status LED.
    bool turnoutCommandReceived()
    {
        return commandSource_.consumeReceived();
    }

private:
    // GPIO 34/35/36/39 are input-only on the ESP32-WROOM-32 and have no
    // internal pull resistor; every other pin does.
    static bool hasInternalPullUp(int pin)
    {
        return pin != 34 && pin != 35 && pin != 36 && pin != 39;
    }

    static TurnoutId globalTurnoutId(const NodeConfig& config, int channel)
    {
        return TurnoutId(config.id().value() * 100 + channel);
    }

    // Debounce window for raw feedback contact reads. Deliberately separate
    // from TurnoutConfig's settleDuration (mechanical settle time) and
    // movementTimeout (fault detection) - smooths electrical noise before
    // the motion state machine ever sees a position.
    static constexpr unsigned long kFeedbackDebounceMs = 20;
    static constexpr unsigned long kLinkRetryMs = 5000;
    static constexpr unsigned long kBrokerLookupIntervalMs = 60000;
    static constexpr unsigned long kPositionRepublishMs = 30000;

    NvsConfigStore configStore_;
    const NodeConfig config_{configStore_.load()};
    ArduinoClock clock_;

    std::array<EspDigitalOutput, 8> outputs_{
        EspDigitalOutput(config_.turnouts()[0].outputPin()),
        EspDigitalOutput(config_.turnouts()[1].outputPin()),
        EspDigitalOutput(config_.turnouts()[2].outputPin()),
        EspDigitalOutput(config_.turnouts()[3].outputPin()),
        EspDigitalOutput(config_.turnouts()[4].outputPin()),
        EspDigitalOutput(config_.turnouts()[5].outputPin()),
        EspDigitalOutput(config_.turnouts()[6].outputPin()),
        EspDigitalOutput(config_.turnouts()[7].outputPin())};

    std::array<EspDigitalInput, 8> inputs_{
        EspDigitalInput(config_.turnouts()[0].feedbackPin(), hasInternalPullUp(config_.turnouts()[0].feedbackPin())),
        EspDigitalInput(config_.turnouts()[1].feedbackPin(), hasInternalPullUp(config_.turnouts()[1].feedbackPin())),
        EspDigitalInput(config_.turnouts()[2].feedbackPin(), hasInternalPullUp(config_.turnouts()[2].feedbackPin())),
        EspDigitalInput(config_.turnouts()[3].feedbackPin(), hasInternalPullUp(config_.turnouts()[3].feedbackPin())),
        EspDigitalInput(config_.turnouts()[4].feedbackPin(), hasInternalPullUp(config_.turnouts()[4].feedbackPin())),
        EspDigitalInput(config_.turnouts()[5].feedbackPin(), hasInternalPullUp(config_.turnouts()[5].feedbackPin())),
        EspDigitalInput(config_.turnouts()[6].feedbackPin(), hasInternalPullUp(config_.turnouts()[6].feedbackPin())),
        EspDigitalInput(config_.turnouts()[7].feedbackPin(), hasInternalPullUp(config_.turnouts()[7].feedbackPin()))};

    WiFiLink wifiLink_{clock_, kLinkRetryMs};

    EspDeviceIdentity deviceIdentity_;
    const std::string macSuffix_{deviceIdentity_.mac().lastFourHexDigits()};
    const NodePresenceTopics presenceTopics_{"mcs/turnout", config_.id().value()};
    EspMdnsResolver mdnsResolver_{"maltbee-turnout-" + macSuffix_};
    BrokerAddressResolver brokerAddressResolver_{mdnsResolver_, clock_, kBrokerLookupIntervalMs};

    MqttLink mqttLink_{
        clock_,
        kLinkRetryMs,
        "maltbee-turnout-" + macSuffix_,
        presenceTopics_.statusTopic(),
        "offline",
        brokerAddressResolver_};

    Loco2MqttPositionReporter positionReporter_{mqttLink_, config_.id().value(), config_.locoNetAddresses()};

    TurnoutRegistry registry_{
        config_.id().value(),
        std::array<Turnout, 8>{
            Turnout(globalTurnoutId(config_, 1), outputs_[0], config_.turnouts()[0].orientation(),
                    FeedbackSensor(inputs_[0], config_.turnouts()[0].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[0].movementTimeout(), config_.turnouts()[0].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 2), outputs_[1], config_.turnouts()[1].orientation(),
                    FeedbackSensor(inputs_[1], config_.turnouts()[1].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[1].movementTimeout(), config_.turnouts()[1].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 3), outputs_[2], config_.turnouts()[2].orientation(),
                    FeedbackSensor(inputs_[2], config_.turnouts()[2].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[2].movementTimeout(), config_.turnouts()[2].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 4), outputs_[3], config_.turnouts()[3].orientation(),
                    FeedbackSensor(inputs_[3], config_.turnouts()[3].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[3].movementTimeout(), config_.turnouts()[3].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 5), outputs_[4], config_.turnouts()[4].orientation(),
                    FeedbackSensor(inputs_[4], config_.turnouts()[4].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[4].movementTimeout(), config_.turnouts()[4].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 6), outputs_[5], config_.turnouts()[5].orientation(),
                    FeedbackSensor(inputs_[5], config_.turnouts()[5].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[5].movementTimeout(), config_.turnouts()[5].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 7), outputs_[6], config_.turnouts()[6].orientation(),
                    FeedbackSensor(inputs_[6], config_.turnouts()[6].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[6].movementTimeout(), config_.turnouts()[6].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 8), outputs_[7], config_.turnouts()[7].orientation(),
                    FeedbackSensor(inputs_[7], config_.turnouts()[7].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[7].movementTimeout(), config_.turnouts()[7].settleDuration()),
                    positionReporter_)}};

    PeriodicPositionRepublisher republisher_{registry_, Duration(kPositionRepublishMs)};

    NodeIdentityGuard identityGuard_{macSuffix_};
    NodeIdentityGate identityGate_{identityGuard_};
    GatedCommandSink gatedSink_{registry_, identityGate_};
    Loco2MqttCommandSource commandSource_{mqttLink_, gatedSink_, config_.id().value(), config_.locoNetAddresses()};
    NodePresenceAnnouncer announcer_{mqttLink_, clock_, presenceTopics_, macSuffix_};

};

#endif
