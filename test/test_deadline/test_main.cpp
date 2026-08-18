#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Deadline.h"

TEST_CASE("A Deadline that has never been armed is not expired")
{
    Deadline deadline;

    REQUIRE_FALSE(deadline.expired(Instant(0)));
    REQUIRE_FALSE(deadline.expired(Instant(1000)));
}

TEST_CASE("armed reflects whether the Deadline has been armed and not disarmed")
{
    Deadline deadline;
    REQUIRE_FALSE(deadline.armed());

    deadline.arm(Instant(100), Duration(50));
    REQUIRE(deadline.armed());

    deadline.disarm();
    REQUIRE_FALSE(deadline.armed());
}

TEST_CASE("An armed Deadline is not expired before its duration elapses")
{
    Deadline deadline;

    deadline.arm(Instant(100), Duration(50));

    REQUIRE_FALSE(deadline.expired(Instant(140)));
}

TEST_CASE("An armed Deadline expires once its duration has elapsed")
{
    Deadline deadline;

    deadline.arm(Instant(100), Duration(50));

    REQUIRE(deadline.expired(Instant(150)));
}

TEST_CASE("A disarmed Deadline is never expired, even past its original deadline")
{
    Deadline deadline;
    deadline.arm(Instant(100), Duration(50));

    deadline.disarm();

    REQUIRE_FALSE(deadline.expired(Instant(200)));
}

TEST_CASE("Re-arming resets the deadline relative to the new arm time")
{
    Deadline deadline;
    deadline.arm(Instant(100), Duration(50));
    REQUIRE(deadline.expired(Instant(150)));

    deadline.arm(Instant(150), Duration(50));

    REQUIRE_FALSE(deadline.expired(Instant(150)));
    REQUIRE(deadline.expired(Instant(200)));
}
