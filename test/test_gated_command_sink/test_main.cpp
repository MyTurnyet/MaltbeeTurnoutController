#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/GatedCommandSink.h"
#include "support/FakeCommandGate.h"
#include "support/FakeTurnoutCommandSink.h"

TEST_CASE("commands pass through while the gate is open")
{
    FakeTurnoutCommandSink inner;
    FakeCommandGate gate(true);
    GatedCommandSink sink(inner, gate);

    sink.command(TurnoutId(301), TurnoutPosition::thrown());

    REQUIRE(inner.commands().size() == 1);
    REQUIRE(inner.commands()[0].first == TurnoutId(301));
    REQUIRE(inner.commands()[0].second == TurnoutPosition::thrown());
}

TEST_CASE("commands are dropped while the gate is closed")
{
    FakeTurnoutCommandSink inner;
    FakeCommandGate gate(false);
    GatedCommandSink sink(inner, gate);

    sink.command(TurnoutId(301), TurnoutPosition::thrown());

    REQUIRE(inner.commands().empty());
}

TEST_CASE("the gate is checked on every command")
{
    FakeTurnoutCommandSink inner;
    FakeCommandGate gate(true);
    GatedCommandSink sink(inner, gate);

    gate.setOpen(false);
    sink.command(TurnoutId(301), TurnoutPosition::closed());
    gate.setOpen(true);
    sink.command(TurnoutId(302), TurnoutPosition::closed());

    REQUIRE(inner.commands().size() == 1);
    REQUIRE(inner.commands()[0].first == TurnoutId(302));
}
