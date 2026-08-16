#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakePositionReporter.h"

TEST_CASE("FakePositionReporter starts with no reports")
{
    FakePositionReporter reporter;

    REQUIRE(reporter.reports().empty());
}

TEST_CASE("FakePositionReporter records a single report")
{
    FakePositionReporter reporter;

    reporter.report(TurnoutId(1), TurnoutState::Closed);

    REQUIRE(reporter.reports().size() == 1);
    REQUIRE(reporter.reports()[0].id == TurnoutId(1));
    REQUIRE(reporter.reports()[0].state == TurnoutState::Closed);
}

TEST_CASE("FakePositionReporter records multiple reports in order")
{
    FakePositionReporter reporter;

    reporter.report(TurnoutId(1), TurnoutState::Moving);
    reporter.report(TurnoutId(1), TurnoutState::Thrown);

    REQUIRE(reporter.reports().size() == 2);
    REQUIRE(reporter.reports()[0].state == TurnoutState::Moving);
    REQUIRE(reporter.reports()[1].state == TurnoutState::Thrown);
}
