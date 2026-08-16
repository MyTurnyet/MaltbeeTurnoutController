#pragma once

#ifdef ARDUINO

#include <Preferences.h>

#include <string>

#include "ports/ConfigStore.h"
#include "domain/NodeConfig.h"
#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutConfig.h"
#include "domain/TurnoutId.h"
#include "domain/Orientation.h"
#include "domain/TurnoutPosition.h"
#include "domain/Level.h"
#include "domain/Duration.h"

class NvsConfigStore : public ConfigStore
{
public:
    void save(const NodeConfig& config) override
    {
        Preferences prefs;
        prefs.begin(kNamespace, false);

        prefs.putInt("id", config.id().value());
        prefs.putString("wssid", config.wifi().ssid().c_str());
        prefs.putString("wpass", config.wifi().password().c_str());
        prefs.putString("bhost", config.broker().host().c_str());
        prefs.putInt("bport", config.broker().port());

        for (int i = 0; i < 8; ++i)
        {
            const TurnoutConfig& turnout = config.turnouts()[i];
            std::string prefix = "t" + std::to_string(i);
            prefs.putInt((prefix + "op").c_str(), turnout.outputPin());
            prefs.putInt((prefix + "fp").c_str(), turnout.feedbackPin());
            prefs.putBool((prefix + "iv").c_str(), isInverted(turnout.orientation()));
            prefs.putULong((prefix + "se").c_str(), turnout.settleDuration().milliseconds());
            prefs.putULong((prefix + "mt").c_str(), turnout.movementTimeout().milliseconds());
        }

        prefs.end();
    }

    NodeConfig load() override
    {
        Preferences prefs;
        prefs.begin(kNamespace, true);

        NodeConfig config = NodeConfig::factoryDefault();

        config = config.withId(NodeId(prefs.getInt("id", config.id().value())));
        config = config.withWifi(WifiCredentials(
            prefs.getString("wssid", config.wifi().ssid().c_str()).c_str(),
            prefs.getString("wpass", config.wifi().password().c_str()).c_str()));
        config = config.withBroker(BrokerAddress(
            prefs.getString("bhost", config.broker().host().c_str()).c_str(),
            prefs.getInt("bport", config.broker().port())));

        for (int i = 0; i < 8; ++i)
        {
            const TurnoutConfig& fallback = config.turnouts()[i];
            std::string prefix = "t" + std::to_string(i);
            int outputPin = prefs.getInt((prefix + "op").c_str(), fallback.outputPin());
            int feedbackPin = prefs.getInt((prefix + "fp").c_str(), fallback.feedbackPin());
            bool inverted = prefs.getBool((prefix + "iv").c_str(), isInverted(fallback.orientation()));
            unsigned long settle = prefs.getULong((prefix + "se").c_str(), fallback.settleDuration().milliseconds());
            unsigned long timeout = prefs.getULong((prefix + "mt").c_str(), fallback.movementTimeout().milliseconds());

            config = config.withTurnout(i, TurnoutConfig(
                fallback.id(),
                outputPin,
                feedbackPin,
                inverted ? Orientation::inverted() : Orientation::normal(),
                Duration(settle),
                Duration(timeout)));
        }

        prefs.end();
        return config;
    }

private:
    static constexpr const char* kNamespace = "mcs-cfg";

    static bool isInverted(Orientation orientation)
    {
        return orientation.toLevel(TurnoutPosition::closed()) == Level::High;
    }
};

#endif
