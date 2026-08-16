#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/PayloadCodec.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutState.h"

TEST_CASE("encode(TurnoutPosition) maps closed and thrown to their payloads")
{
    REQUIRE(PayloadCodec::encode(TurnoutPosition::closed()) == "CLOSED");
    REQUIRE(PayloadCodec::encode(TurnoutPosition::thrown()) == "THROWN");
}

TEST_CASE("decode maps CLOSED/THROWN payloads back to TurnoutPosition")
{
    std::optional<TurnoutPosition> closed = PayloadCodec::decode("CLOSED");
    std::optional<TurnoutPosition> thrown = PayloadCodec::decode("THROWN");

    REQUIRE(closed.has_value());
    REQUIRE(*closed == TurnoutPosition::closed());
    REQUIRE(thrown.has_value());
    REQUIRE(*thrown == TurnoutPosition::thrown());
}

TEST_CASE("decode rejects an unrecognized payload")
{
    REQUIRE_FALSE(PayloadCodec::decode("bogus").has_value());
}

TEST_CASE("encode(TurnoutState) maps every state to JMRI's expected payload")
{
    REQUIRE(PayloadCodec::encode(TurnoutState::Closed) == "CLOSED");
    REQUIRE(PayloadCodec::encode(TurnoutState::Thrown) == "THROWN");
    REQUIRE(PayloadCodec::encode(TurnoutState::Moving) == "INCONSISTENT");
    REQUIRE(PayloadCodec::encode(TurnoutState::Unknown) == "UNKNOWN");
}
