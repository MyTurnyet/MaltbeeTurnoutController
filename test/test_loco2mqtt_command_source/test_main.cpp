#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <array>

#include "adapters/Loco2MqttCommandSource.h"
#include "support/FakeTurnoutCommandSink.h"
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

TEST_CASE("nothing is subscribed until listen is called")
{
    FakeMqttTransport transport;
    FakeTurnoutCommandSink sink;

    Loco2MqttCommandSource source(transport, sink, kNodeId, addresses(1, 41));

    REQUIRE(transport.subscribedTopics.empty());
}

TEST_CASE("listen subscribes the state topic of each configured turnout only")
{
    FakeMqttTransport transport;
    FakeTurnoutCommandSink sink;
    Loco2MqttCommandSource source(transport, sink, kNodeId, addresses(2, 41));

    source.listen();

    REQUIRE(transport.subscribedTopics.size() == 1);
    REQUIRE(transport.subscribedTopics[0] == "loconet/turnout/41/state");
}

TEST_CASE("listen twice does not subscribe twice")
{
    FakeMqttTransport transport;
    FakeTurnoutCommandSink sink;
    Loco2MqttCommandSource source(transport, sink, kNodeId, addresses(2, 41));

    source.listen();
    source.listen();

    REQUIRE(transport.subscribedTopics.size() == 1);
}

TEST_CASE("a state message commands the turnout on the matching channel")
{
    FakeMqttTransport transport;
    FakeTurnoutCommandSink sink;
    Loco2MqttCommandSource source(transport, sink, kNodeId, addresses(2, 41));
    source.listen();

    transport.deliver("loconet/turnout/41/state", "THROWN");
    source.poll();

    REQUIRE(sink.commands().size() == 1);
    REQUIRE(sink.commands()[0].first == TurnoutId(kNodeId * 100 + 2));
    REQUIRE(sink.commands()[0].second == TurnoutPosition::thrown());
}

TEST_CASE("a closed state commands the turnout closed")
{
    FakeMqttTransport transport;
    FakeTurnoutCommandSink sink;
    Loco2MqttCommandSource source(transport, sink, kNodeId, addresses(1, 5));
    source.listen();

    transport.deliver("loconet/turnout/5/state", "CLOSED");
    source.poll();

    REQUIRE(sink.commands()[0].second == TurnoutPosition::closed());
}

TEST_CASE("nothing is commanded until poll is called")
{
    FakeMqttTransport transport;
    FakeTurnoutCommandSink sink;
    Loco2MqttCommandSource source(transport, sink, kNodeId, addresses(1, 5));
    source.listen();

    transport.deliver("loconet/turnout/5/state", "THROWN");

    REQUIRE(sink.commands().empty());
}

TEST_CASE("an unrecognized payload commands nothing")
{
    FakeMqttTransport transport;
    FakeTurnoutCommandSink sink;
    Loco2MqttCommandSource source(transport, sink, kNodeId, addresses(1, 5));
    source.listen();

    transport.deliver("loconet/turnout/5/state", "GARBAGE");
    source.poll();

    REQUIRE(sink.commands().empty());
}

TEST_CASE("a repeated unchanged state is passed on each time, since the turnout ignores no-ops")
{
    FakeMqttTransport transport;
    FakeTurnoutCommandSink sink;
    Loco2MqttCommandSource source(transport, sink, kNodeId, addresses(1, 5));
    source.listen();

    transport.deliver("loconet/turnout/5/state", "THROWN");
    transport.deliver("loconet/turnout/5/state", "THROWN");
    source.poll();

    REQUIRE(sink.commands().size() == 2);
}

TEST_CASE("poll before listen does nothing")
{
    FakeMqttTransport transport;
    FakeTurnoutCommandSink sink;
    Loco2MqttCommandSource source(transport, sink, kNodeId, addresses(1, 5));

    source.poll();

    REQUIRE(sink.commands().empty());
}

TEST_CASE("consumeReceived reports a dispatched command once, then clears")
{
    FakeMqttTransport transport;
    FakeTurnoutCommandSink sink;
    Loco2MqttCommandSource source(transport, sink, kNodeId, addresses(1, 5));
    source.listen();
    transport.deliver("loconet/turnout/5/state", "THROWN");
    source.poll();

    REQUIRE(source.consumeReceived());
    REQUIRE_FALSE(source.consumeReceived());
}

TEST_CASE("consumeReceived stays false when only garbage arrived")
{
    FakeMqttTransport transport;
    FakeTurnoutCommandSink sink;
    Loco2MqttCommandSource source(transport, sink, kNodeId, addresses(1, 5));
    source.listen();
    transport.deliver("loconet/turnout/5/state", "GARBAGE");
    source.poll();

    REQUIRE_FALSE(source.consumeReceived());
}
