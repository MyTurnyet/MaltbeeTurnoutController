#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Debouncer.h"

TEST_CASE("Debouncer reports the initial level before any samples")
{
    Debouncer debouncer(Level::Low, Duration(50));

    REQUIRE(debouncer.stable() == Level::Low);
}

TEST_CASE("A candidate level that has not persisted for the debounce duration does not become stable")
{
    Debouncer debouncer(Level::Low, Duration(50));

    debouncer.sample(Level::High, Instant(0));
    debouncer.sample(Level::High, Instant(30));

    REQUIRE(debouncer.stable() == Level::Low);
}

TEST_CASE("A candidate level that has persisted for at least the debounce duration becomes stable")
{
    Debouncer debouncer(Level::Low, Duration(50));

    debouncer.sample(Level::High, Instant(0));
    debouncer.sample(Level::High, Instant(50));

    REQUIRE(debouncer.stable() == Level::High);
}

TEST_CASE("A brief glitch that reverts before the debounce duration elapses never becomes stable")
{
    Debouncer debouncer(Level::Low, Duration(50));

    debouncer.sample(Level::High, Instant(0));
    debouncer.sample(Level::Low, Instant(20));
    debouncer.sample(Level::Low, Instant(70));

    REQUIRE(debouncer.stable() == Level::Low);
}

TEST_CASE("A new candidate after becoming stable requires its own full debounce period")
{
    Debouncer debouncer(Level::Low, Duration(50));
    debouncer.sample(Level::High, Instant(0));
    debouncer.sample(Level::High, Instant(50));
    REQUIRE(debouncer.stable() == Level::High);

    debouncer.sample(Level::Low, Instant(60));
    REQUIRE(debouncer.stable() == Level::High);

    debouncer.sample(Level::Low, Instant(110));
    REQUIRE(debouncer.stable() == Level::Low);
}
