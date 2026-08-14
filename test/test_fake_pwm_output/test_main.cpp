#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakePwmOutput.h"

TEST_CASE("FakePwmOutput begins at zero duty cycle")
{
    FakePwmOutput output;

    REQUIRE(output.lastDutyCycle() == 0.0);
}

TEST_CASE("writeDutyCycle records the last value written")
{
    FakePwmOutput output;

    output.writeDutyCycle(42.5);

    REQUIRE(output.lastDutyCycle() == 42.5);
}

TEST_CASE("writeDutyCycle overwrites the previous value")
{
    FakePwmOutput output;

    output.writeDutyCycle(10.0);
    output.writeDutyCycle(90.0);

    REQUIRE(output.lastDutyCycle() == 90.0);
}

TEST_CASE("writeDutyCycle records how many times it was called")
{
    FakePwmOutput output;

    output.writeDutyCycle(1.0);
    output.writeDutyCycle(2.0);

    REQUIRE(output.writeCallCount() == 2);
}
