#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/SerialCommissioningAdapter.h"
#include "domain/CommissioningSession.h"
#include "domain/NodeId.h"
#include "support/FakeUartPort.h"
#include "support/FakeConfigStore.h"

TEST_CASE("dispatches a complete line and writes the response")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.feed("id 5\n");
    adapter.poll();

    REQUIRE(uart.written() == "OK\n");
    REQUIRE(session.draft().id() == NodeId(5));
}

TEST_CASE("does not dispatch until a newline arrives")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.feed("id");
    adapter.poll();

    REQUIRE(uart.written().empty());

    uart.feed(" 5\n");
    adapter.poll();

    REQUIRE(uart.written() == "OK\n");
}

TEST_CASE("strips a trailing carriage return")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.feed("id 5\r\n");
    adapter.poll();

    REQUIRE(uart.written() == "OK\n");
    REQUIRE(session.draft().id() == NodeId(5));
}

TEST_CASE("skips blank lines")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.feed("\n");
    adapter.poll();

    REQUIRE(uart.written().empty());
}

TEST_CASE("processes multiple buffered lines in one poll")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.feed("id 5\nid 6\n");
    adapter.poll();

    REQUIRE(uart.written() == "OK\nOK\n");
    REQUIRE(session.draft().id() == NodeId(6));
}

TEST_CASE("reboot sets rebootRequested and stops processing further input")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.feed("reboot\nid 7\n");
    adapter.poll();

    REQUIRE(uart.written() == "REBOOTING\n");
    REQUIRE(adapter.rebootRequested());
    REQUIRE(session.draft().id() != NodeId(7));
}
