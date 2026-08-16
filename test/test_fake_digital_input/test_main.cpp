#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeDigitalInput.h"

TEST_CASE("FakeDigitalInput reads Low by default")
{
    FakeDigitalInput input;

    REQUIRE(input.read() == Level::Low);
}

TEST_CASE("FakeDigitalInput returns enqueued levels in order")
{
    FakeDigitalInput input;
    input.enqueue(Level::High);
    input.enqueue(Level::Low);

    REQUIRE(input.read() == Level::High);
    REQUIRE(input.read() == Level::Low);
}

TEST_CASE("FakeDigitalInput repeats the last read level once the queue is exhausted")
{
    FakeDigitalInput input;
    input.enqueue(Level::High);

    REQUIRE(input.read() == Level::High);
    REQUIRE(input.read() == Level::High);
    REQUIRE(input.read() == Level::High);
}
