#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <utility>

#include "domain/Turnout.h"
#include "support/FakeDigitalOutput.h"
#include "support/FakeDigitalInput.h"
#include "support/FakePositionReporter.h"

TEST_CASE("Turnout reports its id")
{
    FakeDigitalOutput output;
    FakeDigitalInput input;
    Orientation orientation = Orientation::normal();
    FeedbackSensor sensor(input, orientation, Duration(50));
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    FakePositionReporter reporter;
    Turnout turnout(TurnoutId(1), output, orientation, std::move(sensor), std::move(motion), reporter);

    REQUIRE(turnout.id() == TurnoutId(1));
}

TEST_CASE("The first tick reports the initial at-rest state")
{
    FakeDigitalOutput output;
    FakeDigitalInput input;
    Orientation orientation = Orientation::normal();
    FeedbackSensor sensor(input, orientation, Duration(50));
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    FakePositionReporter reporter;
    Turnout turnout(TurnoutId(1), output, orientation, std::move(sensor), std::move(motion), reporter);

    turnout.tick(Instant(0));

    REQUIRE(reporter.reports().size() == 1);
    REQUIRE(reporter.reports()[0].id == TurnoutId(1));
    REQUIRE(reporter.reports()[0].state == TurnoutState::Closed);
}

TEST_CASE("A second tick with no change does not report again")
{
    FakeDigitalOutput output;
    FakeDigitalInput input;
    Orientation orientation = Orientation::normal();
    FeedbackSensor sensor(input, orientation, Duration(50));
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    FakePositionReporter reporter;
    Turnout turnout(TurnoutId(1), output, orientation, std::move(sensor), std::move(motion), reporter);
    turnout.tick(Instant(0));

    turnout.tick(Instant(10));

    REQUIRE(reporter.reports().size() == 1);
}

TEST_CASE("moveTo writes the mapped level to the output")
{
    FakeDigitalOutput output;
    FakeDigitalInput input;
    Orientation orientation = Orientation::normal();
    FeedbackSensor sensor(input, orientation, Duration(50));
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    FakePositionReporter reporter;
    Turnout turnout(TurnoutId(1), output, orientation, std::move(sensor), std::move(motion), reporter);

    turnout.moveTo(TurnoutPosition::thrown(), Instant(0));

    REQUIRE(output.level() == Level::High);
}

TEST_CASE("moveTo commands the motion, which the next tick reports as Moving")
{
    FakeDigitalOutput output;
    FakeDigitalInput input;
    Orientation orientation = Orientation::normal();
    FeedbackSensor sensor(input, orientation, Duration(50));
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    FakePositionReporter reporter;
    Turnout turnout(TurnoutId(1), output, orientation, std::move(sensor), std::move(motion), reporter);
    turnout.moveTo(TurnoutPosition::thrown(), Instant(0));

    turnout.tick(Instant(0));

    REQUIRE(reporter.reports().size() == 1);
    REQUIRE(reporter.reports()[0].state == TurnoutState::Moving);
}

TEST_CASE("Feedback confirming the commanded position eventually settles and reports the new state")
{
    FakeDigitalOutput output;
    FakeDigitalInput input;
    input.enqueue(Level::High);
    Orientation orientation = Orientation::normal();
    FeedbackSensor sensor(input, orientation, Duration(50));
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    FakePositionReporter reporter;
    Turnout turnout(TurnoutId(1), output, orientation, std::move(sensor), std::move(motion), reporter);
    turnout.moveTo(TurnoutPosition::thrown(), Instant(0));

    turnout.tick(Instant(0));
    turnout.tick(Instant(50));
    turnout.tick(Instant(100));

    REQUIRE(reporter.reports().size() == 2);
    REQUIRE(reporter.reports()[0].state == TurnoutState::Moving);
    REQUIRE(reporter.reports()[1].state == TurnoutState::Thrown);
}
