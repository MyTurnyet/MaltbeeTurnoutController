#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/FlashBurst.h"

TEST_CASE("FlashBurst is on immediately at elapsed zero")
{
    FlashBurst burst(Duration(100), Duration(50), 3);
    REQUIRE(burst.levelAt(Duration(0)) == Level::High);
}

TEST_CASE("FlashBurst turns off after onDuration within the first flash")
{
    FlashBurst burst(Duration(100), Duration(50), 3);
    REQUIRE(burst.levelAt(Duration(100)) == Level::Low);
    REQUIRE(burst.levelAt(Duration(149)) == Level::Low);
}

TEST_CASE("FlashBurst turns on again for the second flash")
{
    FlashBurst burst(Duration(100), Duration(50), 3);
    REQUIRE(burst.levelAt(Duration(150)) == Level::High);
    REQUIRE(burst.levelAt(Duration(249)) == Level::High);
}

TEST_CASE("FlashBurst produces exactly flashCount on/off cycles")
{
    // cycle = 150ms. Flash 3 (last): [300,400)=High, [400,450)=Low.
    FlashBurst burst(Duration(100), Duration(50), 3);
    REQUIRE(burst.levelAt(Duration(300)) == Level::High);
    REQUIRE(burst.levelAt(Duration(399)) == Level::High);
    REQUIRE(burst.levelAt(Duration(400)) == Level::Low);
    REQUIRE(burst.levelAt(Duration(449)) == Level::Low);
}

TEST_CASE("FlashBurst stays Low at and after the total burst duration, and never repeats")
{
    // total = 3 * 150ms = 450ms.
    FlashBurst burst(Duration(100), Duration(50), 3);
    REQUIRE(burst.levelAt(Duration(450)) == Level::Low);
    REQUIRE(burst.levelAt(Duration(1000)) == Level::Low);
    REQUIRE(burst.levelAt(Duration(100000)) == Level::Low);
}
