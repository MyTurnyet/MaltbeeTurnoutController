#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TopicScheme.h"
#include "domain/TurnoutId.h"

TEST_CASE("topicFor builds the topic for a turnout id")
{
    REQUIRE(TopicScheme::topicFor(TurnoutId(103)) == "track/turnout/103");
}

TEST_CASE("parse extracts the turnout id from a well-formed topic")
{
    std::optional<TurnoutId> id = TopicScheme::parse("track/turnout/103");

    REQUIRE(id.has_value());
    REQUIRE(*id == TurnoutId(103));
}

TEST_CASE("parse rejects a topic with the wrong prefix")
{
    REQUIRE_FALSE(TopicScheme::parse("some/other/topic").has_value());
}

TEST_CASE("parse rejects a topic with a non-numeric suffix")
{
    REQUIRE_FALSE(TopicScheme::parse("track/turnout/abc").has_value());
}

TEST_CASE("parse rejects a topic with an empty suffix")
{
    REQUIRE_FALSE(TopicScheme::parse("track/turnout/").has_value());
}

TEST_CASE("parse(topicFor(id)) round-trips back to id")
{
    TurnoutId id(217);

    std::optional<TurnoutId> roundTripped = TopicScheme::parse(TopicScheme::topicFor(id));

    REQUIRE(roundTripped.has_value());
    REQUIRE(*roundTripped == id);
}

TEST_CASE("parse returns nullopt for a numeric suffix too large for int, instead of throwing")
{
    REQUIRE(TopicScheme::parse("track/turnout/99999999999") == std::nullopt);
}
