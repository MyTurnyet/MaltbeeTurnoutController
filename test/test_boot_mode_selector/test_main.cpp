#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/BootModeSelector.h"
#include "domain/BootMode.h"
#include "domain/NodeConfig.h"
#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutConfig.h"
#include "domain/TurnoutId.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

namespace
{
NodeConfig fullyValidConfig()
{
    Orientation orientation = Orientation::normal();
    Duration settle(50);
    Duration timeout(200);
    std::array<TurnoutConfig, 8> turnouts{
        TurnoutConfig(TurnoutId(1), 10, 11, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(2), 12, 13, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(3), 14, 15, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(4), 16, 17, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(5), 18, 19, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(6), 20, 21, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(7), 22, 23, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(8), 24, 25, orientation, settle, timeout)};
    return NodeConfig(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), turnouts);
}
}

TEST_CASE("BootModeSelector selects Normal for a fully valid config, no wireless setup request")
{
    REQUIRE(BootModeSelector::select(fullyValidConfig(), false) == BootMode::Normal);
}

TEST_CASE("BootModeSelector selects NeedsCommissioning for the factory default, no wireless setup request")
{
    REQUIRE(BootModeSelector::select(NodeConfig::factoryDefault(), false) == BootMode::NeedsCommissioning);
}

TEST_CASE("BootModeSelector selects NeedsCommissioning for an out-of-range node id alone")
{
    NodeConfig config = fullyValidConfig().withId(NodeId(0));
    REQUIRE(BootModeSelector::select(config, false) == BootMode::NeedsCommissioning);
}

TEST_CASE("BootModeSelector selects NeedsCommissioning for a pin conflict alone")
{
    Orientation orientation = Orientation::normal();
    Duration settle(50);
    Duration timeout(200);
    NodeConfig config = fullyValidConfig().withTurnout(1, TurnoutConfig(TurnoutId(2), 10, 11, orientation, settle, timeout));
    REQUIRE(BootModeSelector::select(config, false) == BootMode::NeedsCommissioning);
}

TEST_CASE("BootModeSelector selects WirelessSetup when requested, even with a fully valid config")
{
    REQUIRE(BootModeSelector::select(fullyValidConfig(), true) == BootMode::WirelessSetup);
}

TEST_CASE("BootModeSelector selects WirelessSetup when requested, even with an invalid config")
{
    REQUIRE(BootModeSelector::select(NodeConfig::factoryDefault(), true) == BootMode::WirelessSetup);
}
