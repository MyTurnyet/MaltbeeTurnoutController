// test/test_button_setup_mode_trigger/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/ButtonSetupModeTrigger.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("ButtonSetupModeTrigger is not requested while the button is still held")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(3000));

    trigger.poll(Instant(0));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger fires on the tick a qualifying long hold is released")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonSetupModeTrigger trigger(bootPin, Duration(3000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(3200));

    REQUIRE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger is edge-triggered - true for one tick only")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    bootPin.enqueue(Level::High);
    ButtonSetupModeTrigger trigger(bootPin, Duration(3000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(3200));
    REQUIRE(trigger.requested());

    trigger.poll(Instant(3400));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger ignores a release before the minimum hold duration")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonSetupModeTrigger trigger(bootPin, Duration(3000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(1500));

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger can fire again on a second qualifying hold and release")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonSetupModeTrigger trigger(bootPin, Duration(3000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(3200));
    REQUIRE(trigger.requested());

    trigger.poll(Instant(4000));
    trigger.poll(Instant(7300));
    REQUIRE(trigger.requested());
}
