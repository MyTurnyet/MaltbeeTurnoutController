#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/NodeId.h"

TEST_CASE("NodeId reports the value it was constructed with")
{
    NodeId id(3);

    REQUIRE(id.value() == 3);
}

TEST_CASE("NodeIds with equal values are equal")
{
    REQUIRE(NodeId(3) == NodeId(3));
    REQUIRE_FALSE(NodeId(3) == NodeId(4));
}

TEST_CASE("NodeIds with different values are not equal")
{
    REQUIRE(NodeId(3) != NodeId(4));
    REQUIRE_FALSE(NodeId(3) != NodeId(3));
}
