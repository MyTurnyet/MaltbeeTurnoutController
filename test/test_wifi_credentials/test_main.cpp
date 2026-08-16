#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/WifiCredentials.h"

TEST_CASE("WifiCredentials reports the ssid and password it was constructed with")
{
    WifiCredentials credentials("MyHomeWifi", "hunter2");

    REQUIRE(credentials.ssid() == "MyHomeWifi");
    REQUIRE(credentials.password() == "hunter2");
}

TEST_CASE("WifiCredentials with equal fields are equal")
{
    REQUIRE(WifiCredentials("ssid", "pw") == WifiCredentials("ssid", "pw"));
    REQUIRE_FALSE(WifiCredentials("ssid", "pw") == WifiCredentials("other", "pw"));
    REQUIRE_FALSE(WifiCredentials("ssid", "pw") == WifiCredentials("ssid", "other"));
}

TEST_CASE("WifiCredentials with different fields are not equal")
{
    REQUIRE(WifiCredentials("ssid", "pw") != WifiCredentials("other", "pw"));
    REQUIRE_FALSE(WifiCredentials("ssid", "pw") != WifiCredentials("ssid", "pw"));
}
