#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <array>

#include "adapters/Loco2MqttPositionReporter.h"
#include "testing/FakeMqttTransport.h"

namespace
{
constexpr int kNodeId = 3;

std::array<int, 8> addresses(int channel, int address)
{
    std::array<int, 8> result{};
    result[channel - 1] = address;
    return result;
}
}

TEST_CASE("a turnout's state is published on its position topic, not retained")
{
    FakeMqttTransport transport;
    Loco2MqttPositionReporter reporter(transport, kNodeId, addresses(2, 41));

    reporter.report(TurnoutId(kNodeId * 100 + 2), TurnoutState::Thrown);

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "mcs/turnout/41/position");
    REQUIRE(transport.published[0].payload == "THROWN");
    REQUIRE_FALSE(transport.published[0].retained);
}

TEST_CASE("closed, moving and unknown states use their own payloads")
{
    FakeMqttTransport transport;
    Loco2MqttPositionReporter reporter(transport, kNodeId, addresses(1, 5));
    const TurnoutId id(kNodeId * 100 + 1);

    reporter.report(id, TurnoutState::Closed);
    reporter.report(id, TurnoutState::Moving);
    reporter.report(id, TurnoutState::Unknown);

    REQUIRE(transport.published[0].payload == "CLOSED");
    REQUIRE(transport.published[1].payload == "INCONSISTENT");
    REQUIRE(transport.published[2].payload == "UNKNOWN");
}

TEST_CASE("a turnout with no LocoNet address publishes nothing")
{
    FakeMqttTransport transport;
    Loco2MqttPositionReporter reporter(transport, kNodeId, addresses(1, 5));

    reporter.report(TurnoutId(kNodeId * 100 + 2), TurnoutState::Closed);

    REQUIRE(transport.published.empty());
}

TEST_CASE("an id belonging to another node publishes nothing")
{
    FakeMqttTransport transport;
    Loco2MqttPositionReporter reporter(transport, kNodeId, addresses(1, 5));

    reporter.report(TurnoutId(4 * 100 + 1), TurnoutState::Closed);

    REQUIRE(transport.published.empty());
}

TEST_CASE("a channel outside 1-8 publishes nothing")
{
    FakeMqttTransport transport;
    Loco2MqttPositionReporter reporter(transport, kNodeId, addresses(1, 5));

    reporter.report(TurnoutId(kNodeId * 100 + 9), TurnoutState::Closed);
    reporter.report(TurnoutId(kNodeId * 100), TurnoutState::Closed);

    REQUIRE(transport.published.empty());
}
