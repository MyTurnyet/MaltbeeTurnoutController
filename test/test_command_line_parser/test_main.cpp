#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/CommandLineParser.h"
#include "domain/ParsedCommand.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/Level.h"
#include "domain/Duration.h"

TEST_CASE("parses id command")
{
    ParsedCommand command = CommandLineParser::parse("id 5");

    REQUIRE(command.type() == CommandType::SetId);
    REQUIRE(command.nodeId() == 5);
}

TEST_CASE("rejects id with a non-numeric argument")
{
    REQUIRE(CommandLineParser::parse("id abc").type() == CommandType::Invalid);
}

TEST_CASE("rejects id with a missing argument")
{
    REQUIRE(CommandLineParser::parse("id").type() == CommandType::Invalid);
}

TEST_CASE("parses wifi command")
{
    ParsedCommand command = CommandLineParser::parse("wifi MySSID MyPass");

    REQUIRE(command.type() == CommandType::SetWifi);
    REQUIRE(command.wifiSsid() == "MySSID");
    REQUIRE(command.wifiPassword() == "MyPass");
}

TEST_CASE("parses broker command")
{
    ParsedCommand command = CommandLineParser::parse("broker 192.168.1.5 1883");

    REQUIRE(command.type() == CommandType::SetBroker);
    REQUIRE(command.brokerHost() == "192.168.1.5");
    REQUIRE(command.brokerPort() == 1883);
}

TEST_CASE("rejects broker with a non-numeric port")
{
    REQUIRE(CommandLineParser::parse("broker host.example.com abc").type() == CommandType::Invalid);
}

TEST_CASE("parses turnout command")
{
    ParsedCommand command = CommandLineParser::parse(
        "turnout 1 pin 13 fb 36 orientation normal settle 50 timeout 200");

    REQUIRE(command.type() == CommandType::SetTurnout);
    REQUIRE(command.turnoutIndex() == 0);
    REQUIRE(command.turnoutConfig().id() == TurnoutId(1));
    REQUIRE(command.turnoutConfig().outputPin() == 13);
    REQUIRE(command.turnoutConfig().feedbackPin() == 36);
    REQUIRE(command.turnoutConfig().settleDuration() == Duration(50));
    REQUIRE(command.turnoutConfig().movementTimeout() == Duration(200));
    REQUIRE(command.turnoutConfig().orientation().toLevel(TurnoutPosition::closed()) == Level::Low);
}

TEST_CASE("parses turnout command with inverted orientation")
{
    ParsedCommand command = CommandLineParser::parse(
        "turnout 8 pin 1 fb 2 orientation inverted settle 10 timeout 20");

    REQUIRE(command.type() == CommandType::SetTurnout);
    REQUIRE(command.turnoutIndex() == 7);
    REQUIRE(command.turnoutConfig().orientation().toLevel(TurnoutPosition::closed()) == Level::High);
}

TEST_CASE("rejects turnout number out of range")
{
    REQUIRE(CommandLineParser::parse(
        "turnout 9 pin 13 fb 36 orientation normal settle 50 timeout 200").type() == CommandType::Invalid);
    REQUIRE(CommandLineParser::parse(
        "turnout 0 pin 13 fb 36 orientation normal settle 50 timeout 200").type() == CommandType::Invalid);
}

TEST_CASE("rejects turnout with unknown orientation")
{
    REQUIRE(CommandLineParser::parse(
        "turnout 1 pin 13 fb 36 orientation sideways settle 50 timeout 200").type() == CommandType::Invalid);
}

TEST_CASE("rejects malformed turnout command")
{
    REQUIRE(CommandLineParser::parse(
        "turnout 1 pinx 13 fb 36 orientation normal settle 50 timeout 200").type() == CommandType::Invalid);
}

TEST_CASE("parses show, save, and reboot commands")
{
    REQUIRE(CommandLineParser::parse("show").type() == CommandType::Show);
    REQUIRE(CommandLineParser::parse("save").type() == CommandType::Save);
    REQUIRE(CommandLineParser::parse("reboot").type() == CommandType::Reboot);
}

TEST_CASE("rejects an empty or blank line")
{
    REQUIRE(CommandLineParser::parse("").type() == CommandType::Invalid);
    REQUIRE(CommandLineParser::parse("   ").type() == CommandType::Invalid);
}

TEST_CASE("rejects an unrecognized command")
{
    REQUIRE(CommandLineParser::parse("banana").type() == CommandType::Invalid);
}
