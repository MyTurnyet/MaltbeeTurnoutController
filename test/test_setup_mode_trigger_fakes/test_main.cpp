#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeSetupModeTrigger.h"
#include "support/FakeDeviceIdentity.h"

TEST_CASE("FakeSetupModeTrigger defaults to not requested")
{
    FakeSetupModeTrigger trigger;
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("FakeSetupModeTrigger reports whatever was set")
{
    FakeSetupModeTrigger trigger;
    trigger.setRequested(true);
    REQUIRE(trigger.requested());
}

TEST_CASE("FakeDeviceIdentity reports the configured MAC")
{
    MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0x3F, 0x2A});
    FakeDeviceIdentity identity(mac);
    REQUIRE(identity.mac() == mac);
}
