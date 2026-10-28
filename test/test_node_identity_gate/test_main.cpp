#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/NodeIdentityGate.h"

TEST_CASE("the gate is open before any mac has been observed")
{
    NodeIdentityGuard guard("AAAA");
    NodeIdentityGate gate(guard);

    REQUIRE(gate.open());
}

TEST_CASE("the gate stays open when only this node's own mac is observed")
{
    NodeIdentityGuard guard("AAAA");
    NodeIdentityGate gate(guard);

    guard.onMacObserved("AAAA");

    REQUIRE(gate.open());
}

TEST_CASE("the gate closes when another node's mac is observed")
{
    NodeIdentityGuard guard("AAAA");
    NodeIdentityGate gate(guard);

    guard.onMacObserved("BBBB");

    REQUIRE_FALSE(gate.open());
}

TEST_CASE("the gate stays closed after the other node goes quiet and our own mac is seen again")
{
    NodeIdentityGuard guard("AAAA");
    NodeIdentityGate gate(guard);
    guard.onMacObserved("BBBB");

    guard.onMacObserved("AAAA");

    REQUIRE_FALSE(gate.open());
}
