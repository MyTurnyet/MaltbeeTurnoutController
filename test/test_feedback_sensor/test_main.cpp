#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/FeedbackSensor.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("FeedbackSensor reports nothing observed before any samples")
{
    FakeDigitalInput input;
    FeedbackSensor sensor(input, Orientation::normal(), Duration(50));

    REQUIRE_FALSE(sensor.observed().has_value());
}

TEST_CASE("FeedbackSensor reports nothing observed before the settle duration has elapsed since the first sample")
{
    FakeDigitalInput input;
    input.enqueue(Level::Low);
    input.enqueue(Level::Low);
    FeedbackSensor sensor(input, Orientation::normal(), Duration(50));

    sensor.sample(Instant(0));
    sensor.sample(Instant(30));

    REQUIRE_FALSE(sensor.observed().has_value());
}

TEST_CASE("FeedbackSensor reports the debounced position, mapped through a normal orientation, once the settle duration has elapsed")
{
    FakeDigitalInput input;
    input.enqueue(Level::Low);
    input.enqueue(Level::Low);
    FeedbackSensor sensor(input, Orientation::normal(), Duration(50));

    sensor.sample(Instant(0));
    sensor.sample(Instant(50));

    REQUIRE(sensor.observed() == TurnoutPosition::closed());
}

TEST_CASE("FeedbackSensor maps through an inverted orientation")
{
    FakeDigitalInput input;
    input.enqueue(Level::High);
    input.enqueue(Level::High);
    FeedbackSensor sensor(input, Orientation::inverted(), Duration(50));

    sensor.sample(Instant(0));
    sensor.sample(Instant(50));

    REQUIRE(sensor.observed() == TurnoutPosition::closed());
}

TEST_CASE("A glitch that reverts before the settle duration elapses does not appear in the observed position")
{
    FakeDigitalInput input;
    input.enqueue(Level::Low);
    input.enqueue(Level::High);
    input.enqueue(Level::Low);
    input.enqueue(Level::Low);
    FeedbackSensor sensor(input, Orientation::normal(), Duration(50));

    sensor.sample(Instant(0));
    sensor.sample(Instant(10));
    sensor.sample(Instant(20));
    sensor.sample(Instant(70));

    REQUIRE(sensor.observed() == TurnoutPosition::closed());
}
