#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeUartPort.h"

TEST_CASE("FakeUartPort has nothing available by default")
{
    FakeUartPort uart;

    REQUIRE_FALSE(uart.available());
}

TEST_CASE("FakeUartPort returns fed bytes in order")
{
    FakeUartPort uart;
    uart.feed("ab");

    REQUIRE(uart.available());
    REQUIRE(uart.read() == 'a');
    REQUIRE(uart.available());
    REQUIRE(uart.read() == 'b');
    REQUIRE_FALSE(uart.available());
}

TEST_CASE("FakeUartPort records written text in order")
{
    FakeUartPort uart;

    uart.write("OK\n");
    uart.write("more\n");

    REQUIRE(uart.written() == "OK\nmore\n");
}
