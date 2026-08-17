// test/test_button_setup_mode_trigger/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/ButtonSetupModeTrigger.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("ButtonSetupModeTrigger is not requested before the boot window elapses")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(2000));

    trigger.poll(Instant(0));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger is requested when BOOT stays low through the whole window")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(1000));
    trigger.poll(Instant(2000));

    REQUIRE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger is never requested if BOOT goes high at any point")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(1000));
    trigger.poll(Instant(2000));

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger requires the full window even if BOOT is held longer")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(2000));

    trigger.poll(Instant(500));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger stays requested once the window has elapsed, across later polls")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(2000));
    REQUIRE(trigger.requested());
}
