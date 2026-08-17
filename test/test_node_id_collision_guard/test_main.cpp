#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/NodeIdCollisionGuard.h"
#include "domain/NodeId.h"

TEST_CASE("NodeIdCollisionGuard treats a message about a different id as unrelated")
{
    NodeIdCollisionGuard guard(NodeId(3));

    REQUIRE(guard.evaluate(NodeId(7), false) == PresenceVerdict::Unrelated);
    REQUIRE(guard.evaluate(NodeId(7), true) == PresenceVerdict::Unrelated);
}

TEST_CASE("NodeIdCollisionGuard treats a same-id message before this node announced as a collision")
{
    NodeIdCollisionGuard guard(NodeId(3));

    REQUIRE(guard.evaluate(NodeId(3), false) == PresenceVerdict::Collision);
}

TEST_CASE("NodeIdCollisionGuard treats a same-id message after this node announced as self")
{
    NodeIdCollisionGuard guard(NodeId(3));

    REQUIRE(guard.evaluate(NodeId(3), true) == PresenceVerdict::Self);
}
