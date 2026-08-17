#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/BlinkOutIdentifier.h"
#include "domain/NodeId.h"

TEST_CASE("BlinkOutIdentifier blinks on immediately at elapsed zero")
{
    BlinkOutIdentifier identifier(NodeId(3), Duration(200), Duration(200), Duration(1000));
    REQUIRE(identifier.levelAt(Duration(0)) == Level::High);
}

TEST_CASE("BlinkOutIdentifier turns off after onDuration within one blink")
{
    BlinkOutIdentifier identifier(NodeId(3), Duration(200), Duration(200), Duration(1000));
    REQUIRE(identifier.levelAt(Duration(250)) == Level::Low);
}

TEST_CASE("BlinkOutIdentifier produces N on/off blinks for id N")
{
    // id=2: blink period 400ms (200 on + 200 off). Blink 1: [0,200)=On [200,400)=Off.
    // Blink 2: [400,600)=On [600,800)=Off. Then pause.
    BlinkOutIdentifier identifier(NodeId(2), Duration(200), Duration(200), Duration(1000));

    REQUIRE(identifier.levelAt(Duration(0)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(199)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(200)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(399)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(400)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(599)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(600)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(799)) == Level::Low);
}

TEST_CASE("BlinkOutIdentifier is off during the pause phase after all blinks")
{
    // id=2: 2 blinks * 400ms = 800ms of blinking, then 1000ms pause -> [800, 1800) = Low.
    BlinkOutIdentifier identifier(NodeId(2), Duration(200), Duration(200), Duration(1000));

    REQUIRE(identifier.levelAt(Duration(800)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(1500)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(1799)) == Level::Low);
}

TEST_CASE("BlinkOutIdentifier repeats the full cycle after the pause")
{
    // id=2: full cycle = 800ms blinking + 1000ms pause = 1800ms.
    BlinkOutIdentifier identifier(NodeId(2), Duration(200), Duration(200), Duration(1000));

    REQUIRE(identifier.levelAt(Duration(1800)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(1800 + 199)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(1800 + 200)) == Level::Low);
}

TEST_CASE("BlinkOutIdentifier scales the blink count with a different id")
{
    // id=1: blink period 200ms (100 on + 100 off).
    BlinkOutIdentifier identifier(NodeId(1), Duration(100), Duration(100), Duration(500));

    REQUIRE(identifier.levelAt(Duration(0)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(100)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(199)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(200)) == Level::Low); // pause phase starts (1 blink * 200ms)
    REQUIRE(identifier.levelAt(Duration(699)) == Level::Low); // still pause (200 + 500 = 700ms cycle)
    REQUIRE(identifier.levelAt(Duration(700)) == Level::High); // cycle repeats
}
