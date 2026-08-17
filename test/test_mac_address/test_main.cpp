#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/MacAddress.h"

TEST_CASE("MacAddress stores and returns the 6 raw bytes")
{
    MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0x3F, 0x2A});

    std::array<uint8_t, 6> bytes = mac.bytes();
    REQUIRE(bytes[0] == 0x24);
    REQUIRE(bytes[5] == 0x2A);
}

TEST_CASE("MacAddress::lastFourHexDigits formats the last two bytes as uppercase hex")
{
    MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0x3F, 0x2A});
    REQUIRE(mac.lastFourHexDigits() == "3F2A");
}

TEST_CASE("MacAddress::lastFourHexDigits zero-pads single-hex-digit bytes")
{
    MacAddress mac({0x00, 0x00, 0x00, 0x00, 0x00, 0x0A});
    REQUIRE(mac.lastFourHexDigits() == "000A");
}

TEST_CASE("MacAddress equality compares all 6 bytes")
{
    MacAddress a({1, 2, 3, 4, 5, 6});
    MacAddress b({1, 2, 3, 4, 5, 6});
    MacAddress c({1, 2, 3, 4, 5, 7});

    REQUIRE(a == b);
    REQUIRE(a != c);
}
