// test/test_setup_ap_name/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/SetupApName.h"
#include "domain/MacAddress.h"

TEST_CASE("SetupApName::from formats Tortoise-Setup-<last 4 hex digits>")
{
    MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0x3F, 0x2A});
    REQUIRE(SetupApName::from(mac) == "Tortoise-Setup-3F2A");
}

TEST_CASE("SetupApName::from zero-pads short hex digits")
{
    MacAddress mac({0x00, 0x00, 0x00, 0x00, 0x00, 0x0A});
    REQUIRE(SetupApName::from(mac) == "Tortoise-Setup-000A");
}
