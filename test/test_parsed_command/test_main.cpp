#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <optional>

#include "domain/ParsedCommand.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutConfig.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

TEST_CASE("ParsedCommand::setId carries the node id")
{
    ParsedCommand command = ParsedCommand::setId(5);

    REQUIRE(command.type() == CommandType::SetId);
    REQUIRE(command.nodeId() == 5);
}

TEST_CASE("ParsedCommand::setWifi carries ssid and password")
{
    ParsedCommand command = ParsedCommand::setWifi("MySSID", "MyPass");

    REQUIRE(command.type() == CommandType::SetWifi);
    REQUIRE(command.wifiSsid() == "MySSID");
    REQUIRE(command.wifiPassword() == "MyPass");
}

TEST_CASE("ParsedCommand::setBroker carries host and port")
{
    ParsedCommand command = ParsedCommand::setBroker("192.168.1.5", 1883);

    REQUIRE(command.type() == CommandType::SetBroker);
    REQUIRE(command.brokerHost() == "192.168.1.5");
    REQUIRE(command.brokerPort() == 1883);
}

TEST_CASE("ParsedCommand::setTurnout carries the index and config")
{
    TurnoutConfig config(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));

    ParsedCommand command = ParsedCommand::setTurnout(0, config);

    REQUIRE(command.type() == CommandType::SetTurnout);
    REQUIRE(command.turnoutIndex() == 0);
    REQUIRE(command.turnoutConfig() == config);
}

TEST_CASE("ParsedCommand::show/save/reboot carry no data")
{
    REQUIRE(ParsedCommand::show().type() == CommandType::Show);
    REQUIRE(ParsedCommand::save().type() == CommandType::Save);
    REQUIRE(ParsedCommand::reboot().type() == CommandType::Reboot);
}

TEST_CASE("ParsedCommand::invalid carries a reason")
{
    ParsedCommand command = ParsedCommand::invalid("bad input");

    REQUIRE(command.type() == CommandType::Invalid);
    REQUIRE(command.invalidReason() == "bad input");
}

TEST_CASE("ParsedCommand::turnoutConfig throws when called on a non-SetTurnout command")
{
    ParsedCommand command = ParsedCommand::setId(5);

    REQUIRE_THROWS_AS(command.turnoutConfig(), std::bad_optional_access);
}
