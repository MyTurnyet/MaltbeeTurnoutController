#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeNodePresenceReporter.h"
#include "domain/NodeId.h"

TEST_CASE("FakeNodePresenceReporter starts with no announcements")
{
    FakeNodePresenceReporter reporter;
    REQUIRE(reporter.announced().empty());
}

TEST_CASE("FakeNodePresenceReporter records announcements in order")
{
    FakeNodePresenceReporter reporter;

    reporter.announce(NodeId(3));
    reporter.announce(NodeId(5));

    REQUIRE(reporter.announced().size() == 2);
    REQUIRE(reporter.announced()[0] == NodeId(3));
    REQUIRE(reporter.announced()[1] == NodeId(5));
}
