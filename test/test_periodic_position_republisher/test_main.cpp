#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Duration.h"
#include "domain/Instant.h"
#include "domain/PeriodicPositionRepublisher.h"
#include "support/FakePositionBroadcaster.h"

namespace
{
const Duration kInterval(30000);
}

TEST_CASE("nothing is republished while disconnected")
{
    FakePositionBroadcaster broadcaster;
    PeriodicPositionRepublisher republisher(broadcaster, kInterval);

    republisher.update(Instant(0), false);
    republisher.update(Instant(100000), false);

    REQUIRE(broadcaster.reportAllCalls() == 0);
}

TEST_CASE("everything is republished as soon as the connection comes up")
{
    FakePositionBroadcaster broadcaster;
    PeriodicPositionRepublisher republisher(broadcaster, kInterval);

    republisher.update(Instant(5000), true);

    REQUIRE(broadcaster.reportAllCalls() == 1);
}

TEST_CASE("nothing is republished again before the interval elapses")
{
    FakePositionBroadcaster broadcaster;
    PeriodicPositionRepublisher republisher(broadcaster, kInterval);
    republisher.update(Instant(5000), true);

    republisher.update(Instant(5000 + 29999), true);

    REQUIRE(broadcaster.reportAllCalls() == 1);
}

TEST_CASE("everything is republished once the interval elapses while connected")
{
    FakePositionBroadcaster broadcaster;
    PeriodicPositionRepublisher republisher(broadcaster, kInterval);
    republisher.update(Instant(5000), true);

    republisher.update(Instant(5000 + 30000), true);

    REQUIRE(broadcaster.reportAllCalls() == 2);
}

TEST_CASE("a republish rearms the interval")
{
    FakePositionBroadcaster broadcaster;
    PeriodicPositionRepublisher republisher(broadcaster, kInterval);
    republisher.update(Instant(0), true);
    republisher.update(Instant(30000), true);

    republisher.update(Instant(30001), true);
    republisher.update(Instant(59999), true);

    REQUIRE(broadcaster.reportAllCalls() == 2);
}

TEST_CASE("a reconnect republishes immediately")
{
    FakePositionBroadcaster broadcaster;
    PeriodicPositionRepublisher republisher(broadcaster, kInterval);
    republisher.update(Instant(0), true);
    republisher.update(Instant(10), false);

    republisher.update(Instant(20), true);

    REQUIRE(broadcaster.reportAllCalls() == 2);
}
