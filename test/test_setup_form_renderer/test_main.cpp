#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

#include "domain/SetupFormRenderer.h"

TEST_CASE("splitIpv4Octets splits a valid dotted quad")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("192.168.1.13");

    REQUIRE(octets[0] == "192");
    REQUIRE(octets[1] == "168");
    REQUIRE(octets[2] == "1");
    REQUIRE(octets[3] == "13");
}

TEST_CASE("splitIpv4Octets returns all-blank for an empty host")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}

TEST_CASE("splitIpv4Octets returns all-blank for a hostname")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("mqtt.local");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}

TEST_CASE("splitIpv4Octets returns all-blank when an octet is out of range")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("192.168.1.999");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}

TEST_CASE("splitIpv4Octets returns all-blank for the wrong number of parts")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("192.168.1");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}
