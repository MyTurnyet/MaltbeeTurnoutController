#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeConfigStore.h"
#include "domain/NodeConfig.h"
#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"

TEST_CASE("FakeConfigStore returns the factory default when nothing has been saved")
{
    FakeConfigStore store;

    REQUIRE(store.load() == NodeConfig::factoryDefault());
}

TEST_CASE("FakeConfigStore returns exactly what was saved")
{
    FakeConfigStore store;
    NodeConfig config = NodeConfig::factoryDefault().withId(NodeId(3)).withWifi(WifiCredentials("ssid", "pw"));

    store.save(config);

    REQUIRE(store.load() == config);
}

TEST_CASE("A second save overwrites the first")
{
    FakeConfigStore store;
    NodeConfig first = NodeConfig::factoryDefault().withId(NodeId(3));
    NodeConfig second = NodeConfig::factoryDefault().withId(NodeId(5));

    store.save(first);
    store.save(second);

    REQUIRE(store.load() == second);
}
