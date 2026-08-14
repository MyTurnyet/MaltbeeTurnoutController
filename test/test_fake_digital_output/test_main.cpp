#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeDigitalOutput.h"

TEST_CASE("FakeDigitalOutput begins low")
{
    FakeDigitalOutput output;

    REQUIRE_FALSE(output.isSet());
}

TEST_CASE("set(true) reports high")
{
    FakeDigitalOutput output;

    output.set(true);

    REQUIRE(output.isSet());
}

TEST_CASE("set(false) reports low")
{
    FakeDigitalOutput output;
    output.set(true);

    output.set(false);

    REQUIRE_FALSE(output.isSet());
}

TEST_CASE("set() records how many times it was called")
{
    FakeDigitalOutput output;

    output.set(true);
    output.set(false);
    output.set(true);

    REQUIRE(output.setCallCount() == 3);
}
