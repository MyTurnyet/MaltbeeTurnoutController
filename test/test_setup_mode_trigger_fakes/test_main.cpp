#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeSetupModeTrigger.h"
#include "support/FakeDeviceIdentity.h"
#include "support/FakeSetupModeRequestStore.h"

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

TEST_CASE("FakeSetupModeRequestStore has no pending request by default")
{
    FakeSetupModeRequestStore store;
    REQUIRE_FALSE(store.consumeRequest());
}

TEST_CASE("FakeSetupModeRequestStore reports and clears a pending request")
{
    FakeSetupModeRequestStore store;
    store.requestOnNextBoot();

    REQUIRE(store.consumeRequest());
    REQUIRE_FALSE(store.consumeRequest());
}
