#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeIdentifyRequestTrigger.h"

TEST_CASE("FakeIdentifyRequestTrigger defaults to not requested")
{
    FakeIdentifyRequestTrigger trigger;
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("FakeIdentifyRequestTrigger reports whatever was set")
{
    FakeIdentifyRequestTrigger trigger;
    trigger.setRequested(true);
    REQUIRE(trigger.requested());
}
