#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "domain/CommissioningSession.h"
#include "domain/ParsedCommand.h"
#include "domain/NodeConfig.h"
#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutConfig.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"
#include "support/FakeConfigStore.h"

namespace
{
TurnoutConfig makeTurnoutConfig(int id, int outputPin, int feedbackPin)
{
    return TurnoutConfig(TurnoutId(id), outputPin, feedbackPin, Orientation::normal(), Duration(50), Duration(200));
}
}

TEST_CASE("CommissioningSession starts from the store's loaded config")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    REQUIRE(session.draft() == NodeConfig::factoryDefault());
}

TEST_CASE("apply(SetId) updates the draft id")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    CommissioningResult result = session.apply(ParsedCommand::setId(5));

    REQUIRE(result.response == "OK");
    REQUIRE_FALSE(result.rebootRequested);
    REQUIRE(session.draft().id() == NodeId(5));
}

TEST_CASE("apply(SetWifi) updates the draft wifi credentials")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    session.apply(ParsedCommand::setWifi("MySSID", "MyPass"));

    REQUIRE(session.draft().wifi() == WifiCredentials("MySSID", "MyPass"));
}

TEST_CASE("apply(SetBroker) updates the draft broker address")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    session.apply(ParsedCommand::setBroker("192.168.1.5", 1883));

    REQUIRE(session.draft().broker() == BrokerAddress("192.168.1.5", 1883));
}

TEST_CASE("apply(SetTurnout) updates only the targeted turnout")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    TurnoutConfig config = makeTurnoutConfig(1, 13, 36);

    session.apply(ParsedCommand::setTurnout(0, config));

    REQUIRE(session.draft().turnouts()[0] == config);
}

TEST_CASE("apply(Show) reports the draft config")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    session.apply(ParsedCommand::setId(3));
    session.apply(ParsedCommand::setWifi("MySSID", "MyPass"));
    session.apply(ParsedCommand::setBroker("host", 1883));

    CommissioningResult result = session.apply(ParsedCommand::show());

    REQUIRE_FALSE(result.rebootRequested);
    REQUIRE(result.response.find("id: 3\n") != std::string::npos);
    REQUIRE(result.response.find("wifi: MySSID\n") != std::string::npos);
    REQUIRE(result.response.find("broker: host:1883\n") != std::string::npos);
    REQUIRE(result.response.find("turnout 1: pin=-1 fb=-1 orientation=normal settle=50 timeout=200\n") != std::string::npos);
}

TEST_CASE("apply(Save) rejects an invalid draft and does not persist it")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    CommissioningResult result = session.apply(ParsedCommand::save());

    REQUIRE(result.response.rfind("ERROR:", 0) == 0);
    REQUIRE_FALSE(result.rebootRequested);
    REQUIRE(store.load() == NodeConfig::factoryDefault());
}

TEST_CASE("apply(Save) persists a valid draft")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    session.apply(ParsedCommand::setId(3));
    for (int i = 0; i < 8; ++i)
    {
        session.apply(ParsedCommand::setTurnout(i, makeTurnoutConfig(i + 1, 100 + i, 200 + i)));
    }

    CommissioningResult result = session.apply(ParsedCommand::save());

    REQUIRE(result.response == "OK: saved");
    REQUIRE_FALSE(result.rebootRequested);
    REQUIRE(store.load().id() == NodeId(3));
}

TEST_CASE("apply(Reboot) requests a reboot without persisting")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    CommissioningResult result = session.apply(ParsedCommand::reboot());

    REQUIRE(result.response == "REBOOTING");
    REQUIRE(result.rebootRequested);
}

TEST_CASE("apply(Invalid) reports the parser's reason")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    CommissioningResult result = session.apply(ParsedCommand::invalid("bad input"));

    REQUIRE(result.response == "ERROR: bad input");
    REQUIRE_FALSE(result.rebootRequested);
}
