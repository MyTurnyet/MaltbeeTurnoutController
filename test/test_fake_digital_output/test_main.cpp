#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeDigitalOutput.h"

TEST_CASE("FakeDigitalOutput begins Low")
{
    FakeDigitalOutput output;

    REQUIRE(output.level() == Level::Low);
}

TEST_CASE("write(High) reports High")
{
    FakeDigitalOutput output;

    output.write(Level::High);

    REQUIRE(output.level() == Level::High);
}

TEST_CASE("write(Low) reports Low")
{
    FakeDigitalOutput output;
    output.write(Level::High);

    output.write(Level::Low);

    REQUIRE(output.level() == Level::Low);
}

TEST_CASE("write() records how many times it was called")
{
    FakeDigitalOutput output;

    output.write(Level::High);
    output.write(Level::Low);
    output.write(Level::High);

    REQUIRE(output.writeCallCount() == 3);
}
