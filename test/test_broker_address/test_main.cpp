#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/BrokerAddress.h"

TEST_CASE("BrokerAddress reports the host and port it was constructed with")
{
    BrokerAddress broker("mqtt.example.com", 1883);

    REQUIRE(broker.host() == "mqtt.example.com");
    REQUIRE(broker.port() == 1883);
}

TEST_CASE("BrokerAddress with equal fields are equal")
{
    REQUIRE(BrokerAddress("host", 1883) == BrokerAddress("host", 1883));
    REQUIRE_FALSE(BrokerAddress("host", 1883) == BrokerAddress("other", 1883));
    REQUIRE_FALSE(BrokerAddress("host", 1883) == BrokerAddress("host", 8883));
}

TEST_CASE("BrokerAddress with different fields are not equal")
{
    REQUIRE(BrokerAddress("host", 1883) != BrokerAddress("host", 8883));
    REQUIRE_FALSE(BrokerAddress("host", 1883) != BrokerAddress("host", 1883));
}
