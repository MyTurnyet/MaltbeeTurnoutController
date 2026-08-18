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

TEST_CASE("expired detects a deadline that has passed across a millis() wraparound")
{
    // millis() (and therefore Instant) is an unsigned 32-bit counter that
    // wraps to 0 after ~49.7 days (2^32 ms). Arm well before the wrap, at a
    // time that itself never overflows...
    Deadline deadline;
    deadline.arm(Instant(4200000000UL), Duration(200));

    // ...then ask about a "now" from long after the counter has actually
    // wrapped around past 2^32 and come back up to a small value. Real
    // elapsed time here is about 26 hours past the deadline.
    REQUIRE(deadline.expired(Instant(32704UL)));
}

TEST_CASE("expired does not report a deadline expired just because now wrapped to a smaller number")
{
    // Same wraparound scenario, but "now" is chosen to be chronologically
    // before the deadline despite being numerically smaller (having
    // already wrapped) than deadline_'s own un-wrapped value.
    Deadline deadline;
    deadline.arm(Instant(4294967200UL), Duration(200)); // deadline_ wraps to 104

    REQUIRE_FALSE(deadline.expired(Instant(54UL))); // true unwrapped time is 50ms before the deadline
}
