#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/MqttTopicRouter.h"

TEST_CASE("dispatch calls the handler registered for the matching topic")
{
    MqttTopicRouter router;
    std::string received;
    router.on("a/topic", [&received](const std::string& payload) { received = payload; });

    router.dispatch("a/topic", "hello");

    REQUIRE(received == "hello");
}

TEST_CASE("dispatch does nothing for an unregistered topic")
{
    MqttTopicRouter router;
    bool called = false;
    router.on("a/topic", [&called](const std::string&) { called = true; });

    router.dispatch("other/topic", "hello");

    REQUIRE_FALSE(called);
}

TEST_CASE("dispatch only calls the handler for the matching topic, not others")
{
    MqttTopicRouter router;
    std::string firstReceived;
    std::string secondReceived;
    router.on("topic/one", [&firstReceived](const std::string& payload) { firstReceived = payload; });
    router.on("topic/two", [&secondReceived](const std::string& payload) { secondReceived = payload; });

    router.dispatch("topic/two", "world");

    REQUIRE(firstReceived.empty());
    REQUIRE(secondReceived == "world");
}
