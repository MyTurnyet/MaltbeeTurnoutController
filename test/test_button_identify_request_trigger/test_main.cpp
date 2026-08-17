// test/test_button_identify_request_trigger/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/ButtonIdentifyRequestTrigger.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("ButtonIdentifyRequestTrigger is not requested while the button is still held")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonIdentifyRequestTrigger fires on the tick a qualifying short press is released")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(150));

    REQUIRE(trigger.requested());
}

TEST_CASE("ButtonIdentifyRequestTrigger is edge-triggered - true for one tick only")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    bootPin.enqueue(Level::High);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(150));
    REQUIRE(trigger.requested());

    trigger.poll(Instant(300));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonIdentifyRequestTrigger ignores a press shorter than the debounce minimum")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(10));

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonIdentifyRequestTrigger ignores a press held longer than the max (that's a setup-mode hold, not a short press)")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(2500));

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonIdentifyRequestTrigger can fire again on a second qualifying short press")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(150));
    REQUIRE(trigger.requested());

    trigger.poll(Instant(1000));
    trigger.poll(Instant(1150));
    REQUIRE(trigger.requested());
}
