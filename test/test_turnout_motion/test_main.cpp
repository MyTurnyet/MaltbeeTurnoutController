#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutMotion.h"

TEST_CASE("A freshly constructed TurnoutMotion reports the initial position at rest")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));

    REQUIRE(motion.state() == TurnoutState::Closed);
}

TEST_CASE("commandTo transitions from AtRest to Moving")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));

    motion.commandTo(TurnoutPosition::thrown(), Instant(0));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("Feedback matching the target while Moving transitions to Settling, still reported as Moving")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));

    motion.update(TurnoutPosition::thrown(), Instant(10));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("Further updates before the settle deadline elapses keep reporting Moving")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(TurnoutPosition::thrown(), Instant(10));

    motion.update(std::nullopt, Instant(40));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("Once the settle deadline expires, TurnoutMotion returns to AtRest reporting the commanded position")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(TurnoutPosition::thrown(), Instant(10));

    motion.update(std::nullopt, Instant(60));

    REQUIRE(motion.state() == TurnoutState::Thrown);
}

TEST_CASE("If the movement timeout expires before feedback confirms the target, TurnoutMotion faults")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));

    motion.update(std::nullopt, Instant(200));

    REQUIRE(motion.state() == TurnoutState::Unknown);
}

TEST_CASE("A faulted TurnoutMotion self-heals to Settling once feedback matches the last commanded target")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(std::nullopt, Instant(200));

    motion.update(TurnoutPosition::thrown(), Instant(210));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("A faulted TurnoutMotion stays faulted if feedback still does not match the target")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(std::nullopt, Instant(200));

    motion.update(TurnoutPosition::closed(), Instant(210));

    REQUIRE(motion.state() == TurnoutState::Unknown);
}

TEST_CASE("Feedback contradicting the at-rest position faults the motion")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));

    motion.update(TurnoutPosition::thrown(), Instant(5));

    REQUIRE(motion.state() == TurnoutState::Unknown);
}

TEST_CASE("Feedback confirming the at-rest position leaves it at rest")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));

    motion.update(TurnoutPosition::closed(), Instant(5));

    REQUIRE(motion.state() == TurnoutState::Closed);
}

TEST_CASE("No observation while at rest does not fault the motion")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));

    motion.update(std::nullopt, Instant(5));

    REQUIRE(motion.state() == TurnoutState::Closed);
}

TEST_CASE("A new command while already moving retargets and re-arms the movement timeout")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));

    motion.commandTo(TurnoutPosition::closed(), Instant(50));
    motion.update(std::nullopt, Instant(200));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("A retargeted movement faults at the new deadline if feedback still has not arrived")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.commandTo(TurnoutPosition::closed(), Instant(50));

    motion.update(std::nullopt, Instant(250));

    REQUIRE(motion.state() == TurnoutState::Unknown);
}

TEST_CASE("A new command while settling interrupts the settle and returns to Moving toward the new target")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(TurnoutPosition::thrown(), Instant(10));

    motion.commandTo(TurnoutPosition::closed(), Instant(20));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("A new command while faulted recovers to Moving toward the new target")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(std::nullopt, Instant(200));

    motion.commandTo(TurnoutPosition::closed(), Instant(210));

    REQUIRE(motion.state() == TurnoutState::Moving);
}
