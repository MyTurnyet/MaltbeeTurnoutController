# NodeConfig & ConfigStore Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the "Node Configuration & Commissioning" groundwork in `docs/software-class-list.md` (backlog item #17) — the `NodeConfig` value object (a node's whole identity/WiFi/broker/turnout configuration, `with...()` updates, `factoryDefault()`, `validate() -> vector<ConfigError>`) and the `ConfigStore` port + `FakeConfigStore` test double it sits behind. This resolves the Value Objects table's still-unbuilt `TurnoutConfig` entry and unblocks the later bench/wireless commissioning tasks (#18/#19), which depend on `NodeConfig`/`ConfigStore` existing.

**Architecture:** Six small, independently-testable classes, built bottom-up so each task only depends on already-merged code: `NodeId` (Task 1, a small wrapper exactly like `TurnoutId`) → `WifiCredentials` (Task 2) → `BrokerAddress` (Task 3) → `TurnoutConfig` (Task 4, composes already-merged `TurnoutId`/`Orientation`/`Duration`) → `NodeConfig` (Task 5, composes Tasks 1–4 plus `ConfigError` and `validate()`/`factoryDefault()`/`with...()`) → `ConfigStore` port + `FakeConfigStore` (Task 6, the first port whose fake needs a real domain value — `NodeConfig` — to hold). All six are pure value objects/ports with no I/O; `ConfigStore` is a driven-side port following the same shape as the existing `Clock`/`DigitalOutput`/`DigitalInput`/`PositionReporter` ports. Following this repo's established value-object style (private fields, public const accessors, `operator==`/`operator!=`) rather than the design doc's illustrative public-field `struct NodeConfig { ... }` sketch — consistency with `TurnoutId`/`TurnoutPosition`/`Duration`/etc. matters more than literal fidelity to what is explicitly pseudocode in the doc.

**Tech Stack:** PlatformIO `native` environment, C++17 (`std::array`, `std::optional`, `std::vector`, `std::string`), Catch2 3.7.1 (vendored, `test_framework = custom`).

## Global Constraints

- Domain code must compile and run under the `native` PlatformIO environment **without** `Arduino.h`. (`CLAUDE.md`)
- No mocking framework — hand-written fakes only. `FakeConfigStore` (Task 6) is this plan's only fake, named `Fake*` per this repo's established renaming of the design doc's test-double names (doc says `InMemoryConfigStore`; repo convention has uniformly renamed every prior test double to `Fake<Port>` — `ManualClock`→`FakeClock`, `ScriptedInput`→`FakeDigitalInput`, `RecordingOutput`→`FakeDigitalOutput`, `CapturingReporter`→`FakePositionReporter` — so this plan continues that pattern rather than using the doc's literal name). (`CLAUDE.md`)
- TDD throughout: failing native test first — actually run it and see it fail — then minimal implementation, then green. Do not write test and implementation together. (`CLAUDE.md`)
- Classes are built needs-driven, not speculatively. (`CLAUDE.md`) `NodeConfig::validate()` implements exactly the two checks the design doc names explicitly — pin conflicts across all 8 turnouts' output/feedback pins, and an out-of-range node id (valid range 1–16, per "Node identity" in Key Resolved Decisions) — not additional speculative rules (e.g. WiFi/broker field validation) the doc's "etc." leaves open but doesn't specify.
- **`NodeConfig::factoryDefault()` is deliberately NOT valid per `validate()`.** A brand-new, uncommissioned node has no real GPIO wiring or credentials yet — `factoryDefault()` uses `NodeId(0)` (below the valid 1–16 range) and pin `-1` for every turnout's output/feedback pin (an unmistakable "unset" sentinel, deliberately identical across all 8 turnouts so the pin-conflict check flags it too). This is intentional: the "Per-Node Workflow" in the design doc requires `show` → fill in real values → `save()`, and `save()` is expected to gate on `validate()` in a later task (`CommissioningSession`, backlog #18, not part of this plan) — a `factoryDefault()` that already passed validation would defeat that gate.
- **`TurnoutConfig::operator==` compares `Orientation` via `orientation().toLevel(TurnoutPosition::closed())` rather than a direct `Orientation::operator==`**, because `Orientation` (already merged, already reviewed) has no equality operator and adding one is out of this plan's scope. This is a complete equality check, not an approximation: `Orientation` has exactly two possible states (`normal`/`inverted`), and `toLevel` at a fixed input (`closed()`) maps them to `Level::Low`/`Level::High` respectively — the two states are fully distinguished by that one probe.
- `native`'s `build_flags` includes `-Ilib/McsCore/src` — use include paths like `"domain/NodeConfig.h"`, `"ports/ConfigStore.h"`, matching existing files.
- Commits in this repo must go through the `/arlo-commits` skill (CLAUDE.md) — never hand-written `git commit`. ACN's "Small Features and Bug Fixes" rule caps any `F`/`B` commit that changes more than 8 lines of code (including tests) at risk level `!` — it cannot be `.`/`^` regardless of test coverage. Every task below is new `F` behavior well over 8 lines, so expect `! F`.
- Every step that changes files ends with `pio test -e native` passing before moving on.
- **Commit often:** each task below ends in its own commit — do not batch multiple tasks into one commit.

---

## Task 1: `NodeId` value object

**Files:**
- Create: `lib/McsCore/src/domain/NodeId.h`
- Test: `test/test_node_id/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `class NodeId` with `explicit NodeId(int value)`, `int value() const`, `operator==`, `operator!=`. Task 5 (`NodeConfig`) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_node_id/test_main.cpp`:

```cpp
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
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_node_id`
Expected: FAIL — compile error, `domain/NodeId.h` does not exist.

- [ ] **Step 3: Write `NodeId`**

Create `lib/McsCore/src/domain/NodeId.h`:

```cpp
#pragma once

class NodeId
{
public:
    explicit NodeId(int value) : value_(value)
    {
    }

    int value() const
    {
        return value_;
    }

    bool operator==(const NodeId& other) const
    {
        return value_ == other.value_;
    }

    bool operator!=(const NodeId& other) const
    {
        return !(*this == other);
    }

private:
    int value_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_node_id`
Expected: PASS — 3 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_node_id`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/domain/NodeId.h` and `test/test_node_id/test_main.cpp` together. Expect `! F`.

---

## Task 2: `WifiCredentials` value object

**Files:**
- Create: `lib/McsCore/src/domain/WifiCredentials.h`
- Test: `test/test_wifi_credentials/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `class WifiCredentials` with `WifiCredentials(std::string ssid, std::string password)`, `const std::string& ssid() const`, `const std::string& password() const`, `operator==`, `operator!=`. Task 5 (`NodeConfig`) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_wifi_credentials/test_main.cpp`:

```cpp
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
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_wifi_credentials`
Expected: FAIL — compile error, `domain/WifiCredentials.h` does not exist.

- [ ] **Step 3: Write `WifiCredentials`**

Create `lib/McsCore/src/domain/WifiCredentials.h`:

```cpp
#pragma once

#include <string>
#include <utility>

class WifiCredentials
{
public:
    WifiCredentials(std::string ssid, std::string password)
        : ssid_(std::move(ssid)), password_(std::move(password))
    {
    }

    const std::string& ssid() const
    {
        return ssid_;
    }

    const std::string& password() const
    {
        return password_;
    }

    bool operator==(const WifiCredentials& other) const
    {
        return ssid_ == other.ssid_ && password_ == other.password_;
    }

    bool operator!=(const WifiCredentials& other) const
    {
        return !(*this == other);
    }

private:
    std::string ssid_;
    std::string password_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_wifi_credentials`
Expected: PASS — 3 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_wifi_credentials`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/domain/WifiCredentials.h` and `test/test_wifi_credentials/test_main.cpp` together. Expect `! F`.

---

## Task 3: `BrokerAddress` value object

**Files:**
- Create: `lib/McsCore/src/domain/BrokerAddress.h`
- Test: `test/test_broker_address/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `class BrokerAddress` with `BrokerAddress(std::string host, int port)`, `const std::string& host() const`, `int port() const`, `operator==`, `operator!=`. Task 5 (`NodeConfig`) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_broker_address/test_main.cpp`:

```cpp
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
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_broker_address`
Expected: FAIL — compile error, `domain/BrokerAddress.h` does not exist.

- [ ] **Step 3: Write `BrokerAddress`**

Create `lib/McsCore/src/domain/BrokerAddress.h`:

```cpp
#pragma once

#include <string>
#include <utility>

class BrokerAddress
{
public:
    BrokerAddress(std::string host, int port)
        : host_(std::move(host)), port_(port)
    {
    }

    const std::string& host() const
    {
        return host_;
    }

    int port() const
    {
        return port_;
    }

    bool operator==(const BrokerAddress& other) const
    {
        return host_ == other.host_ && port_ == other.port_;
    }

    bool operator!=(const BrokerAddress& other) const
    {
        return !(*this == other);
    }

private:
    std::string host_;
    int port_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_broker_address`
Expected: PASS — 3 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_broker_address`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/domain/BrokerAddress.h` and `test/test_broker_address/test_main.cpp` together. Expect `! F`.

---

## Task 4: `TurnoutConfig` value object

**Files:**
- Create: `lib/McsCore/src/domain/TurnoutConfig.h`
- Test: `test/test_turnout_config/test_main.cpp`

**Interfaces:**
- Consumes: `TurnoutId` (`explicit TurnoutId(int)`, `operator==`), `Orientation` (`static normal()`, `toLevel(TurnoutPosition) const`), `TurnoutPosition` (`static closed()`), `Duration` (`explicit Duration(unsigned long)`, `operator==`) — all already merged.
- Produces: `class TurnoutConfig` with `TurnoutConfig(TurnoutId id, int outputPin, int feedbackPin, Orientation orientation, Duration settleDuration, Duration movementTimeout)`, accessors `id()`, `outputPin()`, `feedbackPin()`, `orientation()`, `settleDuration()`, `movementTimeout()`, `operator==`, `operator!=`. Task 5 (`NodeConfig`) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_turnout_config/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutConfig.h"
#include "domain/TurnoutId.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

TEST_CASE("TurnoutConfig reports the fields it was constructed with")
{
    TurnoutConfig config(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));

    REQUIRE(config.id() == TurnoutId(1));
    REQUIRE(config.outputPin() == 13);
    REQUIRE(config.feedbackPin() == 36);
    REQUIRE(config.settleDuration() == Duration(50));
    REQUIRE(config.movementTimeout() == Duration(200));
}

TEST_CASE("TurnoutConfigs with equal fields are equal")
{
    TurnoutConfig a(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));
    TurnoutConfig b(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));

    REQUIRE(a == b);
}

TEST_CASE("TurnoutConfigs differing only by id are not equal")
{
    TurnoutConfig a(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));
    TurnoutConfig b(TurnoutId(2), 13, 36, Orientation::normal(), Duration(50), Duration(200));

    REQUIRE(a != b);
}

TEST_CASE("TurnoutConfigs differing only by output pin are not equal")
{
    TurnoutConfig a(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));
    TurnoutConfig b(TurnoutId(1), 14, 36, Orientation::normal(), Duration(50), Duration(200));

    REQUIRE(a != b);
}

TEST_CASE("TurnoutConfigs differing only by orientation are not equal")
{
    TurnoutConfig a(TurnoutId(1), 13, 36, Orientation::normal(), Duration(50), Duration(200));
    TurnoutConfig b(TurnoutId(1), 13, 36, Orientation::inverted(), Duration(50), Duration(200));

    REQUIRE(a != b);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_turnout_config`
Expected: FAIL — compile error, `domain/TurnoutConfig.h` does not exist.

- [ ] **Step 3: Write `TurnoutConfig`**

Create `lib/McsCore/src/domain/TurnoutConfig.h`:

```cpp
#pragma once

#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

class TurnoutConfig
{
public:
    TurnoutConfig(TurnoutId id,
                  int outputPin,
                  int feedbackPin,
                  Orientation orientation,
                  Duration settleDuration,
                  Duration movementTimeout)
        : id_(id),
          outputPin_(outputPin),
          feedbackPin_(feedbackPin),
          orientation_(orientation),
          settleDuration_(settleDuration),
          movementTimeout_(movementTimeout)
    {
    }

    TurnoutId id() const
    {
        return id_;
    }

    int outputPin() const
    {
        return outputPin_;
    }

    int feedbackPin() const
    {
        return feedbackPin_;
    }

    Orientation orientation() const
    {
        return orientation_;
    }

    Duration settleDuration() const
    {
        return settleDuration_;
    }

    Duration movementTimeout() const
    {
        return movementTimeout_;
    }

    bool operator==(const TurnoutConfig& other) const
    {
        // Orientation has no operator== (out of this task's scope to add);
        // toLevel at a fixed input fully distinguishes its two possible states.
        return id_ == other.id_
            && outputPin_ == other.outputPin_
            && feedbackPin_ == other.feedbackPin_
            && orientation_.toLevel(TurnoutPosition::closed()) == other.orientation_.toLevel(TurnoutPosition::closed())
            && settleDuration_ == other.settleDuration_
            && movementTimeout_ == other.movementTimeout_;
    }

    bool operator!=(const TurnoutConfig& other) const
    {
        return !(*this == other);
    }

private:
    TurnoutId id_;
    int outputPin_;
    int feedbackPin_;
    Orientation orientation_;
    Duration settleDuration_;
    Duration movementTimeout_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_turnout_config`
Expected: PASS — 5 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_turnout_config`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/domain/TurnoutConfig.h` and `test/test_turnout_config/test_main.cpp` together. Expect `! F`.

---

## Task 5: `NodeConfig` value object (+ `ConfigError`)

**Files:**
- Create: `lib/McsCore/src/domain/NodeConfig.h`
- Test: `test/test_node_config/test_main.cpp`

**Interfaces:**
- Consumes: `NodeId` (Task 1), `WifiCredentials` (Task 2), `BrokerAddress` (Task 3), `TurnoutConfig` (Task 4), `TurnoutId`/`Orientation`/`Duration` (already merged).
- Produces: `struct ConfigError { std::string message; operator==; operator!=; }` and `class NodeConfig` with `NodeConfig(NodeId id, WifiCredentials wifi, BrokerAddress broker, std::array<TurnoutConfig, 8> turnouts)`, `static NodeConfig factoryDefault()`, accessors `id()`, `wifi()`, `broker()`, `turnouts()`, `withId(NodeId) const`, `withWifi(WifiCredentials) const`, `withBroker(BrokerAddress) const`, `withTurnout(int index, TurnoutConfig) const`, `std::vector<ConfigError> validate() const`, `operator==`, `operator!=`. Task 6 (`ConfigStore`) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_node_config/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <array>

#include "domain/NodeConfig.h"
#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutConfig.h"
#include "domain/TurnoutId.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

namespace
{
TurnoutConfig makeTurnoutConfig(int id, int outputPin, int feedbackPin)
{
    return TurnoutConfig(TurnoutId(id), outputPin, feedbackPin, Orientation::normal(), Duration(50), Duration(200));
}

std::array<TurnoutConfig, 8> validTurnouts()
{
    return {
        makeTurnoutConfig(1, 100, 200),
        makeTurnoutConfig(2, 101, 201),
        makeTurnoutConfig(3, 102, 202),
        makeTurnoutConfig(4, 103, 203),
        makeTurnoutConfig(5, 104, 204),
        makeTurnoutConfig(6, 105, 205),
        makeTurnoutConfig(7, 106, 206),
        makeTurnoutConfig(8, 107, 207)
    };
}
}

TEST_CASE("NodeConfig reports the fields it was constructed with")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    REQUIRE(config.id() == NodeId(3));
    REQUIRE(config.wifi() == WifiCredentials("ssid", "pw"));
    REQUIRE(config.broker() == BrokerAddress("host", 1883));
    REQUIRE(config.turnouts()[0] == makeTurnoutConfig(1, 100, 200));
    REQUIRE(config.turnouts()[7] == makeTurnoutConfig(8, 107, 207));
}

TEST_CASE("withId returns a new config with only the id changed")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    NodeConfig updated = config.withId(NodeId(5));

    REQUIRE(updated.id() == NodeId(5));
    REQUIRE(updated.wifi() == config.wifi());
    REQUIRE(updated.broker() == config.broker());
    REQUIRE(config.id() == NodeId(3));
}

TEST_CASE("withWifi returns a new config with only the wifi credentials changed")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    NodeConfig updated = config.withWifi(WifiCredentials("newSsid", "newPw"));

    REQUIRE(updated.wifi() == WifiCredentials("newSsid", "newPw"));
    REQUIRE(updated.id() == config.id());
    REQUIRE(config.wifi() == WifiCredentials("ssid", "pw"));
}

TEST_CASE("withBroker returns a new config with only the broker address changed")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    NodeConfig updated = config.withBroker(BrokerAddress("newHost", 8883));

    REQUIRE(updated.broker() == BrokerAddress("newHost", 8883));
    REQUIRE(updated.id() == config.id());
    REQUIRE(config.broker() == BrokerAddress("host", 1883));
}

TEST_CASE("withTurnout replaces exactly the targeted index, others unchanged")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());
    TurnoutConfig replacement = makeTurnoutConfig(2, 999, 998);

    NodeConfig updated = config.withTurnout(1, replacement);

    REQUIRE(updated.turnouts()[1] == replacement);
    REQUIRE(updated.turnouts()[0] == makeTurnoutConfig(1, 100, 200));
    REQUIRE(updated.turnouts()[2] == makeTurnoutConfig(3, 102, 202));
    REQUIRE(config.turnouts()[1] == makeTurnoutConfig(2, 101, 201));
}

TEST_CASE("factoryDefault produces an unconfigured node with 8 sequential turnout ids")
{
    NodeConfig defaultConfig = NodeConfig::factoryDefault();

    REQUIRE(defaultConfig.id() == NodeId(0));
    REQUIRE(defaultConfig.wifi() == WifiCredentials("", ""));
    REQUIRE(defaultConfig.broker() == BrokerAddress("", 1883));
    REQUIRE(defaultConfig.turnouts()[0].id() == TurnoutId(1));
    REQUIRE(defaultConfig.turnouts()[7].id() == TurnoutId(8));
}

TEST_CASE("validate accepts a config with a valid id and no pin conflicts")
{
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    REQUIRE(config.validate().empty());
}

TEST_CASE("validate rejects an out-of-range node id")
{
    NodeConfig tooLow(NodeId(0), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());
    NodeConfig tooHigh(NodeId(17), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    REQUIRE(tooLow.validate().size() == 1);
    REQUIRE(tooLow.validate()[0] == ConfigError{"Node id out of range (must be 1-16)"});
    REQUIRE(tooHigh.validate().size() == 1);
}

TEST_CASE("validate rejects two turnouts claiming the same pin")
{
    std::array<TurnoutConfig, 8> turnouts = validTurnouts();
    turnouts[1] = makeTurnoutConfig(2, 100, 201);
    NodeConfig config(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), turnouts);

    std::vector<ConfigError> errors = config.validate();

    REQUIRE(errors.size() == 1);
    REQUIRE(errors[0] == ConfigError{"Pin 100 used by more than one turnout"});
}

TEST_CASE("factoryDefault fails validate, since it still needs commissioning")
{
    REQUIRE_FALSE(NodeConfig::factoryDefault().validate().empty());
}

TEST_CASE("NodeConfigs with equal fields are equal")
{
    NodeConfig a(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());
    NodeConfig b(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    REQUIRE(a == b);
}

TEST_CASE("NodeConfigs differing only by id are not equal")
{
    NodeConfig a(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());
    NodeConfig b(NodeId(4), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), validTurnouts());

    REQUIRE(a != b);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_node_config`
Expected: FAIL — compile error, `domain/NodeConfig.h` does not exist.

- [ ] **Step 3: Write `NodeConfig`**

Create `lib/McsCore/src/domain/NodeConfig.h`:

```cpp
#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutConfig.h"
#include "domain/TurnoutId.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

struct ConfigError
{
    std::string message;

    bool operator==(const ConfigError& other) const
    {
        return message == other.message;
    }

    bool operator!=(const ConfigError& other) const
    {
        return !(*this == other);
    }
};

class NodeConfig
{
public:
    NodeConfig(NodeId id, WifiCredentials wifi, BrokerAddress broker, std::array<TurnoutConfig, 8> turnouts)
        : id_(id), wifi_(std::move(wifi)), broker_(std::move(broker)), turnouts_(std::move(turnouts))
    {
    }

    static NodeConfig factoryDefault()
    {
        Orientation orientation = Orientation::normal();
        Duration settle(50);
        Duration timeout(200);

        std::array<TurnoutConfig, 8> turnouts{
            TurnoutConfig(TurnoutId(1), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(2), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(3), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(4), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(5), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(6), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(7), -1, -1, orientation, settle, timeout),
            TurnoutConfig(TurnoutId(8), -1, -1, orientation, settle, timeout)
        };

        return NodeConfig(NodeId(0), WifiCredentials("", ""), BrokerAddress("", 1883), turnouts);
    }

    NodeId id() const
    {
        return id_;
    }

    const WifiCredentials& wifi() const
    {
        return wifi_;
    }

    const BrokerAddress& broker() const
    {
        return broker_;
    }

    const std::array<TurnoutConfig, 8>& turnouts() const
    {
        return turnouts_;
    }

    NodeConfig withId(NodeId id) const
    {
        return NodeConfig(id, wifi_, broker_, turnouts_);
    }

    NodeConfig withWifi(WifiCredentials wifi) const
    {
        return NodeConfig(id_, std::move(wifi), broker_, turnouts_);
    }

    NodeConfig withBroker(BrokerAddress broker) const
    {
        return NodeConfig(id_, wifi_, std::move(broker), turnouts_);
    }

    NodeConfig withTurnout(int index, TurnoutConfig turnout) const
    {
        std::array<TurnoutConfig, 8> updated = turnouts_;
        updated[index] = turnout;
        return NodeConfig(id_, wifi_, broker_, updated);
    }

    std::vector<ConfigError> validate() const
    {
        std::vector<ConfigError> errors;

        if (id_.value() < 1 || id_.value() > 16)
        {
            errors.push_back(ConfigError{"Node id out of range (must be 1-16)"});
        }

        std::vector<int> seenPins;
        for (const auto& turnout : turnouts_)
        {
            for (int pin : {turnout.outputPin(), turnout.feedbackPin()})
            {
                bool alreadySeen = std::find(seenPins.begin(), seenPins.end(), pin) != seenPins.end();
                if (alreadySeen)
                {
                    errors.push_back(ConfigError{"Pin " + std::to_string(pin) + " used by more than one turnout"});
                }
                seenPins.push_back(pin);
            }
        }

        return errors;
    }

    bool operator==(const NodeConfig& other) const
    {
        return id_ == other.id_ && wifi_ == other.wifi_ && broker_ == other.broker_ && turnouts_ == other.turnouts_;
    }

    bool operator!=(const NodeConfig& other) const
    {
        return !(*this == other);
    }

private:
    NodeId id_;
    WifiCredentials wifi_;
    BrokerAddress broker_;
    std::array<TurnoutConfig, 8> turnouts_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_node_config`
Expected: PASS — 11 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_node_config`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/domain/NodeConfig.h` and `test/test_node_config/test_main.cpp` together. Expect `! F`.

---

## Task 6: `ConfigStore` port + `FakeConfigStore`

**Files:**
- Create: `lib/McsCore/src/ports/ConfigStore.h`
- Create: `test/support/FakeConfigStore.h`
- Test: `test/test_fake_config_store/test_main.cpp`

**Interfaces:**
- Consumes: `NodeConfig` (Task 5) — `static factoryDefault()`, `operator==`.
- Produces: `class ConfigStore { virtual void save(const NodeConfig&) = 0; virtual NodeConfig load() = 0; };` and `class FakeConfigStore : public ConfigStore` with `void save(const NodeConfig&) override` and `NodeConfig load() override` (returns the last saved config, or `NodeConfig::factoryDefault()` if nothing has been saved yet). A later task (`NvsConfigStore` adapter, Build Order step 11; `CommissioningSession`, backlog #18) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_fake_config_store/test_main.cpp`:

```cpp
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
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_fake_config_store`
Expected: FAIL — compile error, `support/FakeConfigStore.h` does not exist (`ports/ConfigStore.h` doesn't exist either).

- [ ] **Step 3: Write the `ConfigStore` port**

Create `lib/McsCore/src/ports/ConfigStore.h`:

```cpp
#pragma once

#include "domain/NodeConfig.h"

class ConfigStore
{
public:
    virtual ~ConfigStore() = default;
    virtual void save(const NodeConfig& config) = 0;
    virtual NodeConfig load() = 0;
};
```

- [ ] **Step 4: Write `FakeConfigStore`**

Create `test/support/FakeConfigStore.h`:

```cpp
#pragma once

#include <optional>

#include "ports/ConfigStore.h"
#include "domain/NodeConfig.h"

class FakeConfigStore : public ConfigStore
{
public:
    void save(const NodeConfig& config) override
    {
        saved_ = config;
    }

    NodeConfig load() override
    {
        if (saved_.has_value())
        {
            return *saved_;
        }

        return NodeConfig::factoryDefault();
    }

private:
    std::optional<NodeConfig> saved_;
};
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_fake_config_store`
Expected: PASS — 3 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — every test in `test/`, 0 failures.

- [ ] **Step 7: Commit**

Use the `/arlo-commits` skill to commit `lib/McsCore/src/ports/ConfigStore.h`, `test/support/FakeConfigStore.h`, and `test/test_fake_config_store/test_main.cpp` together. Expect `! F`.

---

## Self-Review Notes (for whoever executes this plan)

- **Spec coverage:** `docs/software-class-list.md`'s Value Objects table's `TurnoutConfig` entry (id, output pin, feedback pin, orientation, settle duration, movement timeout) is covered (Task 4). The "Node Configuration & Commissioning" section's `NodeConfig` struct sketch (`NodeId`, `WifiCredentials`, `BrokerAddress`, `array<TurnoutConfig, 8>`), its `with...()` methods (`withId`, `withWifi`, `withTurnout` explicitly named in the doc; `withBroker` added by symmetry, matching `withWifi`), `factoryDefault()`, and `validate() -> vector<ConfigError>` are all covered (Task 5). The `ConfigStore` port from the Ports — Driven Side table (`load()`/`save()`) is covered (Task 6), now persisting a `NodeConfig` rather than a single `TurnoutConfig` as the doc's "Reconciling" section calls for.
- **No placeholders:** all test and production code above is complete, hand-verified (pin-conflict counting logic traced by hand against the test cases; `factoryDefault()`'s deliberate invalidity traced against `validate()`'s two rules), and ready to use verbatim.
- **Type consistency:** each task's constructor/method signatures use exactly the real interfaces defined by earlier tasks — verify none have drifted before starting each task, especially Tasks 4–6 which chain through Tasks 1–3/4/5 respectively.
- **Out of scope, deliberately:** `CommandLineParser`, `CommissioningSession`, `SerialCommissioningAdapter` (backlog #18), `NvsConfigStore` adapter (Build Order step 11), and the wireless commissioning classes (backlog #19/#20) — all needs-driven, not speculative, and all depend on `NodeConfig`/`ConfigStore` existing first, which is exactly what this plan delivers.
