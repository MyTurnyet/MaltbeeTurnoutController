# Bench Serial Commissioning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement backlog #18 (Bench Serial Commissioning) from `docs/software-class-list.md`'s "Node Configuration & Commissioning" section — a USB-serial workflow (`id`/`wifi`/`broker`/`turnout`/`show`/`save`/`reboot`) that lets a bench technician turn a factory-default board into a specific, valid `NodeConfig` without recompiling, using the `NodeConfig`/`ConfigStore` groundwork already built (#17).

**Architecture:** Six small classes, built bottom-up: `ParsedCommand` (Task 1, pure value object — one parsed command line) → `CommandLineParser` (Task 2, pure, text→`ParsedCommand`, the one place untrusted text becomes domain values) → `UartPort` port + `FakeUartPort` (Task 3, driven-side port matching the `DigitalInput`/`DigitalOutput` shape) → `CommissioningSession` (Task 4, holds a draft `NodeConfig` seeded from `ConfigStore::load()`, `apply(ParsedCommand) -> CommissioningResult`, gates `save()` on `NodeConfig::validate()`) → `SerialCommissioningAdapter` (Task 5, buffers `UartPort` bytes into lines, feeds `CommandLineParser`→`CommissioningSession`, writes responses back) → `EspUartPort` (Task 6, the real `Serial`-backed adapter, build-check verified only, no native test). Task 7 updates `docs/task-status.md`.

**Key deviation from the usual adapter split:** `SerialCommissioningAdapter` has **zero** Arduino dependency — it only touches `UartPort&` (a pure port) and `CommissioningSession&` (pure domain). Per `CLAUDE.md`'s stated reason for the `#ifdef ARDUINO` convention ("so they don't break the native build"), that guard exists to protect the native build from real `<Arduino.h>` includes — there is nothing here to protect against. This plan builds it **without** the guard, in `adapters/` (matching the design doc's own layer table) but natively tested with real TDD coverage (`FakeUartPort` + real `CommissioningSession`), rather than only build-check-verified like every other class currently in `adapters/`. Only `EspUartPort` (Task 6), which actually calls `Serial`, needs the guard and the build-check cycle.

**Tech Stack:** PlatformIO `native` environment (Tasks 1–5), `esp32dev` environment (Task 6), C++17, Catch2 3.7.1.

## Global Constraints

- Domain/port code must compile under `native` **without** `Arduino.h` (`CLAUDE.md`). This includes `SerialCommissioningAdapter` in this plan — see the deviation note above.
- No mocking framework — hand-written fakes only (`CLAUDE.md`). `FakeUartPort` (Task 3) is this plan's only new fake, alongside the already-existing `FakeConfigStore`.
- TDD throughout for Tasks 1–5: write the failing native test, actually run it and see it fail, then minimal implementation, then green. Task 6 has no native equivalent (real `Serial` I/O) — verified via the established **build-check cycle** instead (`pio run -e esp32dev` must compile+link, with temporary `src/main.cpp` wiring to force the file into the build, reverted after — same procedure used throughout backlog #15).
- Classes are built needs-driven (`CLAUDE.md`). `CommissioningSession::apply()` implements exactly the seven commands the design doc names (`id`/`wifi`/`broker`/`turnout`/`show`/`save`/`reboot`) — no speculative extras (no `help`, no partial-turnout-field updates, etc.).
- **`NodeConfig::withTurnout`'s missing bounds check (`docs/task-status.md`'s "Known scaffolding debt") is resolved by this plan, not by touching `NodeConfig` itself.** `CommandLineParser::parse` is the boundary where untrusted external text becomes a domain value (`CLAUDE.md`: "Only validate at system boundaries") — it rejects `turnout <n> ...` where `n` is outside 1–8 as `CommandType::Invalid` before a `TurnoutId`/index is ever constructed. `CommissioningSession` and `NodeConfig::withTurnout` only ever see an already-validated index, so no redundant check is added inside `NodeConfig` itself (`CLAUDE.md`: "Trust internal code... only validate at system boundaries"). Task 7 updates the scaffolding-debt note accordingly.
- `reboot` does not auto-save. The per-node workflow (`docs/software-class-list.md`) is explicitly `save` then `reboot`; `CommissioningSession::apply(reboot)` only returns a `rebootRequested` signal — it does not call `ConfigStore::save()`. If the operator reboots without saving, nothing new persists; that is their responsibility, not the session's to enforce.
- **This plan does not wire `SerialCommissioningAdapter`/`EspUartPort` into `ControllerNode`/`main.cpp`.** Backlog #18's own scope (per `docs/task-status.md`) is the three domain/adapter classes plus their supporting port — same split as #15 (build the real ESP32 adapters) vs. #16 (wire them into the composition root). There is no boot-mode-selection logic yet to decide when bench-commissioning should run instead of normal operation; that question belongs to whatever wires this in later.
- `native`'s `build_flags` includes `-Ilib/McsCore/src` — use include paths like `"domain/ParsedCommand.h"`, `"ports/UartPort.h"`, matching existing files.
- Commits go through the `/arlo-commits` skill (`CLAUDE.md`) — never hand-written `git commit`. **ACN's 8-LoC cap for `F`/`B` commits applies regardless of test coverage** — every task below adds well over 8 lines of new behavior, so even the fully native-tested Tasks 1–5 are `! F` (risky, real change), not `^ F`. Only Task 7 (docs-only) is `. d`.
- Every step that changes files ends with `pio test -e native` passing (Tasks 1–5) before moving on. Task 6 ends with `pio run -e esp32dev` passing instead.
- **Commit often:** each task ends in its own commit — do not batch multiple tasks into one commit.

---

## Task 1: `ParsedCommand` value object

**Files:**
- Create: `lib/McsCore/src/domain/ParsedCommand.h`
- Test: `test/test_parsed_command/test_main.cpp`

**Interfaces:**
- Consumes: `TurnoutConfig` (`lib/McsCore/src/domain/TurnoutConfig.h`, already built).
- Produces: `enum class CommandType { SetId, SetWifi, SetBroker, SetTurnout, Show, Save, Reboot, Invalid };` and `class ParsedCommand` with static factories `setId(int)`, `setWifi(std::string, std::string)`, `setBroker(std::string, int)`, `setTurnout(int index, TurnoutConfig)`, `show()`, `save()`, `reboot()`, `invalid(std::string reason)`; accessors `type()`, `nodeId()`, `wifiSsid()`, `wifiPassword()`, `brokerHost()`, `brokerPort()`, `turnoutIndex()`, `turnoutConfig()`, `invalidReason()`. Task 2 (`CommandLineParser`) and Task 4 (`CommissioningSession`) depend on this exact interface. Each accessor besides `type()` is only meaningful when `type()` matches the factory that set it — `turnoutConfig()` calls `.value()` on an internal `optional<TurnoutConfig>` and throws `std::bad_optional_access` if misused, rather than invoking undefined behavior.

- [ ] **Step 1: Write the failing test**

Create `test/test_parsed_command/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/ParsedCommand.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutConfig.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

TEST_CASE("ParsedCommand::setId carries the node id")
{
    ParsedCommand command = ParsedCommand::setId(5);

    REQUIRE(command.type() == CommandType::SetId);
    REQUIRE(command.nodeId() == 5);
}

TEST_CASE("ParsedCommand::setWifi carries ssid and password")
{
    ParsedCommand command = ParsedCommand::setWifi("MySSID", "MyPass");

    REQUIRE(command.type() == CommandType::SetWifi);
    REQUIRE(command.wifiSsid() == "MySSID");
    REQUIRE(command.wifiPassword() == "MyPass");
}

TEST_CASE("ParsedCommand::setBroker carries host and port")
{
    ParsedCommand command = ParsedCommand::setBroker("192.168.1.5", 1883);

    REQUIRE(command.type() == CommandType::SetBroker);
    REQUIRE(command.brokerHost() == "192.168.1.5");
    REQUIRE(command.brokerPort() == 1883);
}

TEST_CASE("ParsedCommand::setTurnout carries the index and config")
{
    TurnoutConfig config(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));

    ParsedCommand command = ParsedCommand::setTurnout(0, config);

    REQUIRE(command.type() == CommandType::SetTurnout);
    REQUIRE(command.turnoutIndex() == 0);
    REQUIRE(command.turnoutConfig() == config);
}

TEST_CASE("ParsedCommand::show/save/reboot carry no data")
{
    REQUIRE(ParsedCommand::show().type() == CommandType::Show);
    REQUIRE(ParsedCommand::save().type() == CommandType::Save);
    REQUIRE(ParsedCommand::reboot().type() == CommandType::Reboot);
}

TEST_CASE("ParsedCommand::invalid carries a reason")
{
    ParsedCommand command = ParsedCommand::invalid("bad input");

    REQUIRE(command.type() == CommandType::Invalid);
    REQUIRE(command.invalidReason() == "bad input");
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_parsed_command`
Expected: FAIL — compile error, `domain/ParsedCommand.h` does not exist.

- [ ] **Step 3: Write `ParsedCommand`**

Create `lib/McsCore/src/domain/ParsedCommand.h`:

```cpp
#pragma once

#include <optional>
#include <string>
#include <utility>

#include "domain/TurnoutConfig.h"

enum class CommandType
{
    SetId,
    SetWifi,
    SetBroker,
    SetTurnout,
    Show,
    Save,
    Reboot,
    Invalid
};

class ParsedCommand
{
public:
    static ParsedCommand setId(int nodeId)
    {
        ParsedCommand command(CommandType::SetId);
        command.nodeId_ = nodeId;
        return command;
    }

    static ParsedCommand setWifi(std::string ssid, std::string password)
    {
        ParsedCommand command(CommandType::SetWifi);
        command.wifiSsid_ = std::move(ssid);
        command.wifiPassword_ = std::move(password);
        return command;
    }

    static ParsedCommand setBroker(std::string host, int port)
    {
        ParsedCommand command(CommandType::SetBroker);
        command.brokerHost_ = std::move(host);
        command.brokerPort_ = port;
        return command;
    }

    static ParsedCommand setTurnout(int index, TurnoutConfig config)
    {
        ParsedCommand command(CommandType::SetTurnout);
        command.turnoutIndex_ = index;
        command.turnoutConfig_ = std::move(config);
        return command;
    }

    static ParsedCommand show()
    {
        return ParsedCommand(CommandType::Show);
    }

    static ParsedCommand save()
    {
        return ParsedCommand(CommandType::Save);
    }

    static ParsedCommand reboot()
    {
        return ParsedCommand(CommandType::Reboot);
    }

    static ParsedCommand invalid(std::string reason)
    {
        ParsedCommand command(CommandType::Invalid);
        command.invalidReason_ = std::move(reason);
        return command;
    }

    CommandType type() const
    {
        return type_;
    }

    int nodeId() const
    {
        return nodeId_;
    }

    const std::string& wifiSsid() const
    {
        return wifiSsid_;
    }

    const std::string& wifiPassword() const
    {
        return wifiPassword_;
    }

    const std::string& brokerHost() const
    {
        return brokerHost_;
    }

    int brokerPort() const
    {
        return brokerPort_;
    }

    int turnoutIndex() const
    {
        return turnoutIndex_;
    }

    const TurnoutConfig& turnoutConfig() const
    {
        return turnoutConfig_.value();
    }

    const std::string& invalidReason() const
    {
        return invalidReason_;
    }

private:
    explicit ParsedCommand(CommandType type) : type_(type)
    {
    }

    CommandType type_;
    int nodeId_ = 0;
    std::string wifiSsid_;
    std::string wifiPassword_;
    std::string brokerHost_;
    int brokerPort_ = 0;
    int turnoutIndex_ = 0;
    std::optional<TurnoutConfig> turnoutConfig_;
    std::string invalidReason_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_parsed_command`
Expected: PASS — 6 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_parsed_command`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/domain/ParsedCommand.h` and `test/test_parsed_command/test_main.cpp` together. Expect `! F`.

---

## Task 2: `CommandLineParser` domain class

**Files:**
- Create: `lib/McsCore/src/domain/CommandLineParser.h`
- Test: `test/test_command_line_parser/test_main.cpp`

**Interfaces:**
- Consumes: `ParsedCommand`/`CommandType` (Task 1), `TurnoutId`, `TurnoutConfig`, `Orientation`, `Duration` (all already built).
- Produces: `class CommandLineParser` with `static ParsedCommand parse(const std::string& line)`. Task 5 (`SerialCommissioningAdapter`) depends on this exact signature.

- [ ] **Step 1: Write the failing test**

Create `test/test_command_line_parser/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/CommandLineParser.h"
#include "domain/ParsedCommand.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/Level.h"
#include "domain/Duration.h"

TEST_CASE("parses id command")
{
    ParsedCommand command = CommandLineParser::parse("id 5");

    REQUIRE(command.type() == CommandType::SetId);
    REQUIRE(command.nodeId() == 5);
}

TEST_CASE("rejects id with a non-numeric argument")
{
    REQUIRE(CommandLineParser::parse("id abc").type() == CommandType::Invalid);
}

TEST_CASE("rejects id with a missing argument")
{
    REQUIRE(CommandLineParser::parse("id").type() == CommandType::Invalid);
}

TEST_CASE("parses wifi command")
{
    ParsedCommand command = CommandLineParser::parse("wifi MySSID MyPass");

    REQUIRE(command.type() == CommandType::SetWifi);
    REQUIRE(command.wifiSsid() == "MySSID");
    REQUIRE(command.wifiPassword() == "MyPass");
}

TEST_CASE("parses broker command")
{
    ParsedCommand command = CommandLineParser::parse("broker 192.168.1.5 1883");

    REQUIRE(command.type() == CommandType::SetBroker);
    REQUIRE(command.brokerHost() == "192.168.1.5");
    REQUIRE(command.brokerPort() == 1883);
}

TEST_CASE("rejects broker with a non-numeric port")
{
    REQUIRE(CommandLineParser::parse("broker host.example.com abc").type() == CommandType::Invalid);
}

TEST_CASE("parses turnout command")
{
    ParsedCommand command = CommandLineParser::parse(
        "turnout 1 pin 13 fb 36 orientation normal settle 50 timeout 200");

    REQUIRE(command.type() == CommandType::SetTurnout);
    REQUIRE(command.turnoutIndex() == 0);
    REQUIRE(command.turnoutConfig().id() == TurnoutId(1));
    REQUIRE(command.turnoutConfig().outputPin() == 13);
    REQUIRE(command.turnoutConfig().feedbackPin() == 36);
    REQUIRE(command.turnoutConfig().settleDuration() == Duration(50));
    REQUIRE(command.turnoutConfig().movementTimeout() == Duration(200));
    REQUIRE(command.turnoutConfig().orientation().toLevel(TurnoutPosition::closed()) == Level::Low);
}

TEST_CASE("parses turnout command with inverted orientation")
{
    ParsedCommand command = CommandLineParser::parse(
        "turnout 8 pin 1 fb 2 orientation inverted settle 10 timeout 20");

    REQUIRE(command.type() == CommandType::SetTurnout);
    REQUIRE(command.turnoutIndex() == 7);
    REQUIRE(command.turnoutConfig().orientation().toLevel(TurnoutPosition::closed()) == Level::High);
}

TEST_CASE("rejects turnout number out of range")
{
    REQUIRE(CommandLineParser::parse(
        "turnout 9 pin 13 fb 36 orientation normal settle 50 timeout 200").type() == CommandType::Invalid);
    REQUIRE(CommandLineParser::parse(
        "turnout 0 pin 13 fb 36 orientation normal settle 50 timeout 200").type() == CommandType::Invalid);
}

TEST_CASE("rejects turnout with unknown orientation")
{
    REQUIRE(CommandLineParser::parse(
        "turnout 1 pin 13 fb 36 orientation sideways settle 50 timeout 200").type() == CommandType::Invalid);
}

TEST_CASE("rejects malformed turnout command")
{
    REQUIRE(CommandLineParser::parse(
        "turnout 1 pinx 13 fb 36 orientation normal settle 50 timeout 200").type() == CommandType::Invalid);
}

TEST_CASE("parses show, save, and reboot commands")
{
    REQUIRE(CommandLineParser::parse("show").type() == CommandType::Show);
    REQUIRE(CommandLineParser::parse("save").type() == CommandType::Save);
    REQUIRE(CommandLineParser::parse("reboot").type() == CommandType::Reboot);
}

TEST_CASE("rejects an empty or blank line")
{
    REQUIRE(CommandLineParser::parse("").type() == CommandType::Invalid);
    REQUIRE(CommandLineParser::parse("   ").type() == CommandType::Invalid);
}

TEST_CASE("rejects an unrecognized command")
{
    REQUIRE(CommandLineParser::parse("banana").type() == CommandType::Invalid);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_command_line_parser`
Expected: FAIL — compile error, `domain/CommandLineParser.h` does not exist.

- [ ] **Step 3: Write `CommandLineParser`**

Create `lib/McsCore/src/domain/CommandLineParser.h`:

```cpp
#pragma once

#include <exception>
#include <sstream>
#include <string>
#include <vector>

#include "domain/ParsedCommand.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutConfig.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

class CommandLineParser
{
public:
    static ParsedCommand parse(const std::string& line)
    {
        std::vector<std::string> tokens = tokenize(line);

        if (tokens.empty())
        {
            return ParsedCommand::invalid("empty command");
        }

        const std::string& keyword = tokens[0];

        if (keyword == "id" && tokens.size() == 2)
        {
            return parseSetId(tokens);
        }
        if (keyword == "wifi" && tokens.size() == 3)
        {
            return ParsedCommand::setWifi(tokens[1], tokens[2]);
        }
        if (keyword == "broker" && tokens.size() == 3)
        {
            return parseSetBroker(tokens);
        }
        if (keyword == "turnout" && tokens.size() == 12)
        {
            return parseSetTurnout(tokens);
        }
        if (keyword == "show" && tokens.size() == 1)
        {
            return ParsedCommand::show();
        }
        if (keyword == "save" && tokens.size() == 1)
        {
            return ParsedCommand::save();
        }
        if (keyword == "reboot" && tokens.size() == 1)
        {
            return ParsedCommand::reboot();
        }

        return ParsedCommand::invalid("unrecognized command: " + line);
    }

private:
    static std::vector<std::string> tokenize(const std::string& line)
    {
        std::vector<std::string> tokens;
        std::istringstream stream(line);
        std::string token;
        while (stream >> token)
        {
            tokens.push_back(token);
        }
        return tokens;
    }

    static bool parseInt(const std::string& text, int& value)
    {
        try
        {
            size_t consumed = 0;
            value = std::stoi(text, &consumed);
            return consumed == text.size();
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    static ParsedCommand parseSetId(const std::vector<std::string>& tokens)
    {
        int value;
        if (!parseInt(tokens[1], value))
        {
            return ParsedCommand::invalid("id: expected an integer");
        }
        return ParsedCommand::setId(value);
    }

    static ParsedCommand parseSetBroker(const std::vector<std::string>& tokens)
    {
        int port;
        if (!parseInt(tokens[2], port))
        {
            return ParsedCommand::invalid("broker: expected an integer port");
        }
        return ParsedCommand::setBroker(tokens[1], port);
    }

    static ParsedCommand parseSetTurnout(const std::vector<std::string>& tokens)
    {
        // turnout <n> pin <gpio> fb <gpio> orientation <normal|inverted> settle <ms> timeout <ms>
        //    0     1   2    3    4   5         6            7             8    9      10     11
        if (tokens[2] != "pin" || tokens[4] != "fb" || tokens[6] != "orientation"
            || tokens[8] != "settle" || tokens[10] != "timeout")
        {
            return ParsedCommand::invalid("turnout: malformed command");
        }

        int n;
        int outputPin;
        int feedbackPin;
        int settleMs;
        int timeoutMs;
        if (!parseInt(tokens[1], n) || !parseInt(tokens[3], outputPin) || !parseInt(tokens[5], feedbackPin)
            || !parseInt(tokens[9], settleMs) || !parseInt(tokens[11], timeoutMs))
        {
            return ParsedCommand::invalid("turnout: expected integers for n/pin/fb/settle/timeout");
        }

        if (n < 1 || n > 8)
        {
            return ParsedCommand::invalid("turnout: n must be 1-8");
        }

        Orientation orientation = Orientation::normal();
        if (tokens[7] == "inverted")
        {
            orientation = Orientation::inverted();
        }
        else if (tokens[7] != "normal")
        {
            return ParsedCommand::invalid("turnout: orientation must be normal or inverted");
        }

        TurnoutConfig config(TurnoutId(n), outputPin, feedbackPin, orientation, Duration(settleMs), Duration(timeoutMs));
        return ParsedCommand::setTurnout(n - 1, config);
    }
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_command_line_parser`
Expected: PASS — 13 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_parsed_command`/`test_command_line_parser`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/domain/CommandLineParser.h` and `test/test_command_line_parser/test_main.cpp` together. Expect `! F`.

---

## Task 3: `UartPort` port + `FakeUartPort`

**Files:**
- Create: `lib/McsCore/src/ports/UartPort.h`
- Create: `test/support/FakeUartPort.h`
- Test: `test/test_fake_uart_port/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `class UartPort` with `virtual bool available() = 0`, `virtual char read() = 0`, `virtual void write(const std::string&) = 0`. `class FakeUartPort : public UartPort` with `void feed(const std::string&)` (queues bytes for `read()`) and `const std::string& written() const` (everything passed to `write()` so far, in order). Task 5 (`SerialCommissioningAdapter`) and Task 6 (`EspUartPort`) depend on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_fake_uart_port/test_main.cpp`:

```cpp
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
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_fake_uart_port`
Expected: FAIL — compile error, `ports/UartPort.h`/`support/FakeUartPort.h` do not exist.

- [ ] **Step 3: Write `UartPort` and `FakeUartPort`**

Create `lib/McsCore/src/ports/UartPort.h`:

```cpp
#pragma once

#include <string>

class UartPort
{
public:
    virtual ~UartPort() = default;
    virtual bool available() = 0;
    virtual char read() = 0;
    virtual void write(const std::string& text) = 0;
};
```

Create `test/support/FakeUartPort.h`:

```cpp
#pragma once

#include <deque>
#include <string>

#include "ports/UartPort.h"

class FakeUartPort : public UartPort
{
public:
    void feed(const std::string& text)
    {
        for (char c : text)
        {
            queue_.push_back(c);
        }
    }

    bool available() override
    {
        return !queue_.empty();
    }

    char read() override
    {
        char c = queue_.front();
        queue_.pop_front();
        return c;
    }

    void write(const std::string& text) override
    {
        written_ += text;
    }

    const std::string& written() const
    {
        return written_;
    }

private:
    std::deque<char> queue_;
    std::string written_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_fake_uart_port`
Expected: PASS — 3 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_fake_uart_port`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/ports/UartPort.h`, `test/support/FakeUartPort.h`, and `test/test_fake_uart_port/test_main.cpp` together. Expect `! F`.

---

## Task 4: `CommissioningSession` domain class

**Files:**
- Create: `lib/McsCore/src/domain/CommissioningSession.h`
- Test: `test/test_commissioning_session/test_main.cpp`

**Interfaces:**
- Consumes: `ConfigStore` (`lib/McsCore/src/ports/ConfigStore.h`), `NodeConfig`/`ConfigError` (`lib/McsCore/src/domain/NodeConfig.h`), `NodeId`, `WifiCredentials`, `BrokerAddress`, `Orientation`, `Level`, `TurnoutPosition`, `TurnoutConfig` (all already built), `ParsedCommand`/`CommandType` (Task 1).
- Produces: `struct CommissioningResult { std::string response; bool rebootRequested; };` and `class CommissioningSession` with `explicit CommissioningSession(ConfigStore&)`, `const NodeConfig& draft() const`, `CommissioningResult apply(const ParsedCommand&)`. Task 5 (`SerialCommissioningAdapter`) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_commissioning_session/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "domain/CommissioningSession.h"
#include "domain/ParsedCommand.h"
#include "domain/NodeConfig.h"
#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutConfig.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"
#include "support/FakeConfigStore.h"

namespace
{
TurnoutConfig makeTurnoutConfig(int id, int outputPin, int feedbackPin)
{
    return TurnoutConfig(TurnoutId(id), outputPin, feedbackPin, Orientation::normal(), Duration(50), Duration(200));
}
}

TEST_CASE("CommissioningSession starts from the store's loaded config")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    REQUIRE(session.draft() == NodeConfig::factoryDefault());
}

TEST_CASE("apply(SetId) updates the draft id")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    CommissioningResult result = session.apply(ParsedCommand::setId(5));

    REQUIRE(result.response == "OK");
    REQUIRE_FALSE(result.rebootRequested);
    REQUIRE(session.draft().id() == NodeId(5));
}

TEST_CASE("apply(SetWifi) updates the draft wifi credentials")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    session.apply(ParsedCommand::setWifi("MySSID", "MyPass"));

    REQUIRE(session.draft().wifi() == WifiCredentials("MySSID", "MyPass"));
}

TEST_CASE("apply(SetBroker) updates the draft broker address")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    session.apply(ParsedCommand::setBroker("192.168.1.5", 1883));

    REQUIRE(session.draft().broker() == BrokerAddress("192.168.1.5", 1883));
}

TEST_CASE("apply(SetTurnout) updates only the targeted turnout")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    TurnoutConfig config = makeTurnoutConfig(1, 13, 36);

    session.apply(ParsedCommand::setTurnout(0, config));

    REQUIRE(session.draft().turnouts()[0] == config);
}

TEST_CASE("apply(Show) reports the draft config")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    session.apply(ParsedCommand::setId(3));
    session.apply(ParsedCommand::setWifi("MySSID", "MyPass"));
    session.apply(ParsedCommand::setBroker("host", 1883));

    CommissioningResult result = session.apply(ParsedCommand::show());

    REQUIRE_FALSE(result.rebootRequested);
    REQUIRE(result.response.find("id: 3\n") != std::string::npos);
    REQUIRE(result.response.find("wifi: MySSID\n") != std::string::npos);
    REQUIRE(result.response.find("broker: host:1883\n") != std::string::npos);
    REQUIRE(result.response.find("turnout 1: pin=-1 fb=-1 orientation=normal settle=50 timeout=200\n") != std::string::npos);
}

TEST_CASE("apply(Save) rejects an invalid draft and does not persist it")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    CommissioningResult result = session.apply(ParsedCommand::save());

    REQUIRE(result.response.rfind("ERROR:", 0) == 0);
    REQUIRE_FALSE(result.rebootRequested);
    REQUIRE(store.load() == NodeConfig::factoryDefault());
}

TEST_CASE("apply(Save) persists a valid draft")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    session.apply(ParsedCommand::setId(3));
    for (int i = 0; i < 8; ++i)
    {
        session.apply(ParsedCommand::setTurnout(i, makeTurnoutConfig(i + 1, 100 + i, 200 + i)));
    }

    CommissioningResult result = session.apply(ParsedCommand::save());

    REQUIRE(result.response == "OK: saved");
    REQUIRE_FALSE(result.rebootRequested);
    REQUIRE(store.load().id() == NodeId(3));
}

TEST_CASE("apply(Reboot) requests a reboot without persisting")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    CommissioningResult result = session.apply(ParsedCommand::reboot());

    REQUIRE(result.response == "REBOOTING");
    REQUIRE(result.rebootRequested);
}

TEST_CASE("apply(Invalid) reports the parser's reason")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    CommissioningResult result = session.apply(ParsedCommand::invalid("bad input"));

    REQUIRE(result.response == "ERROR: bad input");
    REQUIRE_FALSE(result.rebootRequested);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_commissioning_session`
Expected: FAIL — compile error, `domain/CommissioningSession.h` does not exist.

- [ ] **Step 3: Write `CommissioningSession`**

Create `lib/McsCore/src/domain/CommissioningSession.h`:

```cpp
#pragma once

#include <string>
#include <vector>

#include "domain/ParsedCommand.h"
#include "domain/NodeConfig.h"
#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutConfig.h"
#include "domain/Orientation.h"
#include "domain/Level.h"
#include "domain/TurnoutPosition.h"
#include "ports/ConfigStore.h"

struct CommissioningResult
{
    std::string response;
    bool rebootRequested;
};

class CommissioningSession
{
public:
    explicit CommissioningSession(ConfigStore& store)
        : store_(store), draft_(store.load())
    {
    }

    const NodeConfig& draft() const
    {
        return draft_;
    }

    CommissioningResult apply(const ParsedCommand& command)
    {
        switch (command.type())
        {
        case CommandType::SetId:
            draft_ = draft_.withId(NodeId(command.nodeId()));
            return {"OK", false};

        case CommandType::SetWifi:
            draft_ = draft_.withWifi(WifiCredentials(command.wifiSsid(), command.wifiPassword()));
            return {"OK", false};

        case CommandType::SetBroker:
            draft_ = draft_.withBroker(BrokerAddress(command.brokerHost(), command.brokerPort()));
            return {"OK", false};

        case CommandType::SetTurnout:
            draft_ = draft_.withTurnout(command.turnoutIndex(), command.turnoutConfig());
            return {"OK", false};

        case CommandType::Show:
            return {formatShow(), false};

        case CommandType::Save:
            return applySave();

        case CommandType::Reboot:
            return {"REBOOTING", true};

        case CommandType::Invalid:
            return {"ERROR: " + command.invalidReason(), false};
        }

        return {"ERROR: unhandled command", false};
    }

private:
    CommissioningResult applySave()
    {
        std::vector<ConfigError> errors = draft_.validate();
        if (!errors.empty())
        {
            std::string response = "ERROR: ";
            for (size_t i = 0; i < errors.size(); ++i)
            {
                if (i > 0)
                {
                    response += "; ";
                }
                response += errors[i].message;
            }
            return {response, false};
        }

        store_.save(draft_);
        return {"OK: saved", false};
    }

    std::string formatShow() const
    {
        std::string text;
        text += "id: " + std::to_string(draft_.id().value()) + "\n";
        text += "wifi: " + draft_.wifi().ssid() + "\n";
        text += "broker: " + draft_.broker().host() + ":" + std::to_string(draft_.broker().port()) + "\n";

        for (int i = 0; i < 8; ++i)
        {
            const TurnoutConfig& turnout = draft_.turnouts()[i];
            text += "turnout " + std::to_string(i + 1) + ": pin=" + std::to_string(turnout.outputPin())
                + " fb=" + std::to_string(turnout.feedbackPin())
                + " orientation=" + (isInverted(turnout.orientation()) ? "inverted" : "normal")
                + " settle=" + std::to_string(turnout.settleDuration().milliseconds())
                + " timeout=" + std::to_string(turnout.movementTimeout().milliseconds())
                + "\n";
        }

        return text;
    }

    static bool isInverted(Orientation orientation)
    {
        return orientation.toLevel(TurnoutPosition::closed()) == Level::High;
    }

    ConfigStore& store_;
    NodeConfig draft_;
};
```

`isInverted` mirrors `NvsConfigStore`'s private helper of the same name and logic (`lib/McsCore/src/adapters/NvsConfigStore.h`) — `Orientation` has no `operator==`/inverted accessor (out of scope to add per the `NodeConfig` plan), so both places that need to distinguish normal/inverted for display or storage probe it the same way, via a fixed `toLevel` input.

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_commissioning_session`
Expected: PASS — 10 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_commissioning_session`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/domain/CommissioningSession.h` and `test/test_commissioning_session/test_main.cpp` together. Expect `! F`.

---

## Task 5: `SerialCommissioningAdapter`

**Files:**
- Create: `lib/McsCore/src/adapters/SerialCommissioningAdapter.h`
- Test: `test/test_serial_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes: `UartPort` (Task 3), `CommissioningSession`/`CommissioningResult` (Task 4), `CommandLineParser` (Task 2), `ParsedCommand` (Task 1).
- Produces: `class SerialCommissioningAdapter` with `SerialCommissioningAdapter(UartPort&, CommissioningSession&)`, `void poll()`, `bool rebootRequested() const`. No native equivalent needed for a real caller — see Global Constraints for why this file has **no** `#ifdef ARDUINO` guard, unlike every other file in `adapters/`.

- [ ] **Step 1: Write the failing test**

Create `test/test_serial_commissioning_adapter/test_main.cpp`:

```cpp
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
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_serial_commissioning_adapter`
Expected: FAIL — compile error, `adapters/SerialCommissioningAdapter.h` does not exist.

- [ ] **Step 3: Write `SerialCommissioningAdapter`**

Create `lib/McsCore/src/adapters/SerialCommissioningAdapter.h`:

```cpp
#pragma once

#include <string>

#include "ports/UartPort.h"
#include "domain/CommissioningSession.h"
#include "domain/CommandLineParser.h"
#include "domain/ParsedCommand.h"

class SerialCommissioningAdapter
{
public:
    SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session)
        : uart_(uart), session_(session)
    {
    }

    void poll()
    {
        while (uart_.available())
        {
            char c = uart_.read();

            if (c == '\n')
            {
                handleLine(buffer_);
                buffer_.clear();

                if (rebootRequested_)
                {
                    return;
                }
            }
            else if (c != '\r')
            {
                buffer_.push_back(c);
            }
        }
    }

    bool rebootRequested() const
    {
        return rebootRequested_;
    }

private:
    void handleLine(const std::string& line)
    {
        if (line.empty())
        {
            return;
        }

        ParsedCommand command = CommandLineParser::parse(line);
        CommissioningResult result = session_.apply(command);

        uart_.write(result.response + "\n");

        if (result.rebootRequested)
        {
            rebootRequested_ = true;
        }
    }

    UartPort& uart_;
    CommissioningSession& session_;
    std::string buffer_;
    bool rebootRequested_ = false;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_serial_commissioning_adapter`
Expected: PASS — 6 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_serial_commissioning_adapter`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/adapters/SerialCommissioningAdapter.h` and `test/test_serial_commissioning_adapter/test_main.cpp` together. Expect `! F`.

---

## Task 6: `EspUartPort` real adapter

**Files:**
- Create: `lib/McsCore/src/adapters/EspUartPort.h`
- Temporarily modify (then revert): `src/main.cpp`

**Interfaces:**
- Consumes: `UartPort` (Task 3).
- Produces: `class EspUartPort : public UartPort` with `explicit EspUartPort(unsigned long baudRate)`. Not wired into `main.cpp` permanently by this task — see Global Constraints.

This task has no native equivalent (real `Serial` I/O) — follow the **build-check cycle** established in backlog #15: temporarily wire the new class into `src/main.cpp` so PlatformIO's dependency finder pulls it into the `esp32dev` build, confirm `pio run -e esp32dev` compiles and links, then revert `src/main.cpp` back to its current committed content before committing.

- [ ] **Step 1: Write `EspUartPort`**

Create `lib/McsCore/src/adapters/EspUartPort.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include <string>

#include "ports/UartPort.h"

class EspUartPort : public UartPort
{
public:
    explicit EspUartPort(unsigned long baudRate)
    {
        Serial.begin(baudRate);
    }

    bool available() override
    {
        return Serial.available() > 0;
    }

    char read() override
    {
        return static_cast<char>(Serial.read());
    }

    void write(const std::string& text) override
    {
        Serial.print(text.c_str());
    }
};

#endif
```

- [ ] **Step 2: Temporarily wire it into `src/main.cpp` to force compilation**

Record the current committed content of `src/main.cpp` (you will restore it exactly in Step 4). Add a temporary include and a file-scope construction, e.g. append near the top:

```cpp
#include "adapters/EspUartPort.h"
```

and inside `setup()` (after the existing `node->begin();` line), temporarily add:

```cpp
    static EspUartPort debugUart(115200);
    (void)debugUart;
```

- [ ] **Step 3: Run the build-check**

Run: `pio run -e esp32dev`
Expected: SUCCESS — compiles and links with no errors.

If it fails, fix `EspUartPort.h` (not the temporary wiring) and re-run until it succeeds.

- [ ] **Step 4: Revert the temporary `main.cpp` wiring**

Restore `src/main.cpp` to exactly the content recorded in Step 2 (remove the temporary include and the two temporary lines in `setup()`). Run `pio run -e esp32dev` once more to confirm it still builds with `main.cpp` back to its original state (this also exercises `EspUartPort.h` in isolation via the library dependency scan, per `lib_ldf_mode = deep+`).

- [ ] **Step 5: Run the native suite to confirm no regression**

Run: `pio test -e native`
Expected: PASS — unchanged from before this task (`EspUartPort.h` is entirely `#ifdef ARDUINO`-guarded, invisible to the native build).

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/adapters/EspUartPort.h` only (`src/main.cpp` should show no diff, having been reverted to its original content). Expect `! F` — new behavior, no native test coverage possible (real hardware I/O), verified only by the build-check cycle.

---

## Task 7: Update `docs/task-status.md`

**Files:**
- Modify: `docs/task-status.md`

- [ ] **Step 1: Move backlog #18 into the Completed table**

Add a row to the Completed table (after the `ControllerNode` row) summarizing what was built: `ParsedCommand`, `CommandLineParser`, `UartPort`/`FakeUartPort`, `CommissioningSession`, `SerialCommissioningAdapter`, `EspUartPort` — citing the actual commit hashes from Tasks 1–6 (fill in after they exist). Note that `SerialCommissioningAdapter`/`EspUartPort` are not yet wired into `ControllerNode`/`main.cpp` (mirrors the #15→#16 split — no boot-mode-selection logic exists yet to decide when bench-commissioning should run instead of normal operation).

- [ ] **Step 2: Remove backlog #18 from the Backlog table and its Task-details section**

Delete the `#18` row from the Backlog table and its `**#18 — ...**` details paragraph. Leave `#19`/`#20` as-is (their numbers and "Blocked by" columns don't change — `#19` was already blocked only by `#18`, which is now done, so update `#19`'s "Blocked by" column to `—` (unblocked)).

- [ ] **Step 3: Update "Known scaffolding debt"**

Remove the `NodeConfig::withTurnout` bounds-check bullet (resolved — `CommandLineParser::parse` now rejects out-of-range turnout numbers before an index ever reaches `NodeConfig::withTurnout`; see Task 2). Leave the other three bullets (`TopicScheme::parse` — already resolved, verify it's not still listed as open; `ControllerNode`'s debounce/retry literals; factory-default pin `-1` untested) as-is if still accurate — re-read the current file content before editing, since it may have already changed since this plan was written.

- [ ] **Step 4: Update the native test binary count**

The line noting "25 native test binaries pass as of this snapshot" should become 30 (adds `test_parsed_command`, `test_command_line_parser`, `test_fake_uart_port`, `test_commissioning_session`, `test_serial_commissioning_adapter` — `test_esp32_build_check` and `EspUartPort` are build-check-only, not native binaries).

- [ ] **Step 5: Commit**

Use the `/arlo-commits` skill to commit `docs/task-status.md`. Expect `. d` (docs-only, provably safe).
