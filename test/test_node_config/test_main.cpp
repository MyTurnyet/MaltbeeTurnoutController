#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <array>

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
TurnoutConfig makeTurnoutConfig(int id, int outputPin, int feedbackPin)
{
    return TurnoutConfig(TurnoutId(id), outputPin, feedbackPin, Orientation::normal(), Duration(50), Duration(200));
}

std::array<TurnoutConfig, 8> validTurnouts()
{
    return {
        makeTurnoutConfig(1, 100, 200),
        makeTurnoutConfig(2, 101, 201),
        makeTurnoutConfig(3, 102, 202),
        makeTurnoutConfig(4, 103, 203),
        makeTurnoutConfig(5, 104, 204),
        makeTurnoutConfig(6, 105, 205),
        makeTurnoutConfig(7, 106, 206),
        makeTurnoutConfig(8, 107, 207)
    };
}
}

TEST_CASE("NodeConfig reports the fields it was constructed with")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    REQUIRE(config.id() == NodeId(3));
    REQUIRE(config.wifi() == WifiCredentials("ssid", "pw"));
    REQUIRE(config.broker() == BrokerAddress("host", 1883));
    REQUIRE(config.turnouts()[0] == makeTurnoutConfig(1, 100, 200));
    REQUIRE(config.turnouts()[7] == makeTurnoutConfig(8, 107, 207));
}

TEST_CASE("withId returns a new config with only the id changed")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    NodeConfig updated = config.withId(NodeId(5));

    REQUIRE(updated.id() == NodeId(5));
    REQUIRE(updated.wifi() == config.wifi());
    REQUIRE(updated.broker() == config.broker());
    REQUIRE(config.id() == NodeId(3));
}

TEST_CASE("withWifi returns a new config with only the wifi credentials changed")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    NodeConfig updated = config.withWifi(WifiCredentials("newSsid", "newPw"));

    REQUIRE(updated.wifi() == WifiCredentials("newSsid", "newPw"));
    REQUIRE(updated.id() == config.id());
    REQUIRE(config.wifi() == WifiCredentials("ssid", "pw"));
}

TEST_CASE("withBroker returns a new config with only the broker address changed")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    NodeConfig updated = config.withBroker(BrokerAddress("newHost", 8883));

    REQUIRE(updated.broker() == BrokerAddress("newHost", 8883));
    REQUIRE(updated.id() == config.id());
    REQUIRE(config.broker() == BrokerAddress("host", 1883));
}

TEST_CASE("withTurnout replaces exactly the targeted index, others unchanged")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());
    TurnoutConfig replacement = makeTurnoutConfig(2, 999, 998);

    NodeConfig updated = config.withTurnout(1, replacement);

    REQUIRE(updated.turnouts()[1] == replacement);
    REQUIRE(updated.turnouts()[0] == makeTurnoutConfig(1, 100, 200));
    REQUIRE(updated.turnouts()[2] == makeTurnoutConfig(3, 102, 202));
    REQUIRE(config.turnouts()[1] == makeTurnoutConfig(2, 101, 201));
}

TEST_CASE("factoryDefault produces an unconfigured node with 8 sequential turnout ids")
{
    NodeConfig defaultConfig = NodeConfig::factoryDefault();

    REQUIRE(defaultConfig.id() == NodeId(0));
    REQUIRE(defaultConfig.wifi() == WifiCredentials("", ""));
    REQUIRE(defaultConfig.broker() == BrokerAddress("", 1883));
    REQUIRE(defaultConfig.turnouts()[0].id() == TurnoutId(1));
    REQUIRE(defaultConfig.turnouts()[7].id() == TurnoutId(8));
}

TEST_CASE("validate accepts a config with a valid id and no pin conflicts")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    REQUIRE(config.validate().empty());
}

TEST_CASE("validate rejects an out-of-range node id")
{
    NodeConfig tooLow(NodeId(0), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());
    NodeConfig tooHigh(NodeId(17), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    REQUIRE(tooLow.validate().size() == 1);
    REQUIRE(tooLow.validate()[0] == ConfigError{"Node id out of range (must be 1-16)"});
    REQUIRE(tooHigh.validate().size() == 1);
}

TEST_CASE("validate rejects two turnouts claiming the same pin")
{
    std::array<TurnoutConfig, 8> turnouts = validTurnouts();
    turnouts[1] = makeTurnoutConfig(2, 100, 201);
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), turnouts);

    std::vector<ConfigError> errors = config.validate();

    REQUIRE(errors.size() == 1);
    REQUIRE(errors[0] == ConfigError{"Pin 100 used by more than one turnout"});
}

TEST_CASE("validate does not flag sentinel pin -1 as a conflict")
{
    NodeConfig defaultConfig = NodeConfig::factoryDefault();

    std::vector<ConfigError> errors = defaultConfig.validate();

    REQUIRE(errors.size() == 1);
    REQUIRE(errors[0] == ConfigError{"Node id out of range (must be 1-16)"});
}

TEST_CASE("factoryDefault fails validate, since it still needs commissioning")
{
    REQUIRE_FALSE(NodeConfig::factoryDefault().validate().empty());
}

TEST_CASE("NodeConfigs with equal fields are equal")
{
    NodeConfig a(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());
    NodeConfig b(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    REQUIRE(a == b);
}

TEST_CASE("NodeConfigs differing only by id are not equal")
{
    NodeConfig a(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());
    NodeConfig b(NodeId(4), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    REQUIRE(a != b);
}
