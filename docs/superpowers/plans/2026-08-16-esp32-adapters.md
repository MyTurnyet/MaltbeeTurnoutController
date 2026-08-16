# ESP32 Adapters (Build Order 11) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement backlog item #15 (`docs/task-status.md`) — the eight ESP32 hardware/network adapters from `docs/software-class-list.md`'s Adapters table (`ArduinoClock`, `EspDigitalOutput`, `EspDigitalInput`, `NvsConfigStore`, `WiFiLink`, `MqttLink`, `MqttCommandSource`, `MqttPositionReporter`), plus the two pieces of scaffolding debt task-status.md calls out as belonging to this step: removing the unused `PwmOutput` port and fixing `TopicScheme::parse`'s unguarded `std::stoi` before `MqttCommandSource` becomes its first real caller. This is the last domain-adjacent work before backlog #16 (`ControllerNode` + `main.cpp` composition root) can wire a real board together.

**Architecture:** Ten small, independently-committed tasks, ordered so each only depends on already-merged code: cleanup first (`PwmOutput` removal, `TopicScheme::parse` guard — both pure and native-testable), then the four simplest hardware adapters (`ArduinoClock`, `EspDigitalOutput`, `EspDigitalInput`, `NvsConfigStore`), then the two connection-lifecycle adapters (`WiFiLink`, `MqttLink`), then the two adapters that depend on `MqttLink` (`MqttCommandSource`, `MqttPositionReporter`). Every adapter is header-only (matching every existing class in `lib/McsCore/src/`) and guarded `#ifdef ARDUINO`, so it is invisible to the `native` build. Because these adapters wrap real hardware/network APIs (`digitalWrite`, `Preferences`, `WiFi`, `PubSubClient`) with no native equivalent, they cannot get a native TDD red/green cycle like every prior class in this repo — instead each adapter task substitutes a **build-check cycle**: temporarily reference the new header from `src/main.cpp`, run `pio run -e esp32dev` to prove it compiles and links against the real ESP32 toolchain, then revert `src/main.cpp` (its real wiring is backlog #16's job, not this plan's). `pio test -e esp32dev` was tried and rejected for this — it hangs waiting on serial port/device detection even with `--without-uploading --without-testing`, and this project has no ESP32 attached; `pio run -e esp32dev` alone (confirmed: builds cleanly in ~38s against the already-cached `espressif32@7.0.1` platform + `PubSubClient@2.8`) is the right tool for a build-only check.

**Tech Stack:** PlatformIO `esp32dev` environment (`platform = espressif32`, `board = esp32dev`, `framework = arduino`), Arduino-ESP32 core (`WiFi.h`, `Preferences.h`), `knolleary/PubSubClient@^2.8` (already in `platformio.ini`'s `lib_deps`). Cleanup tasks use the existing `native` environment, C++17, Catch2.

## Global Constraints

- Domain code must compile and run under the `native` PlatformIO environment **without** `Arduino.h`; adapters are guarded `#ifdef ARDUINO` so they compile only under `esp32dev`. (`CLAUDE.md`)
- No mocking framework. Adapters in this plan have no fake/double of their own — they *are* the real implementation the existing fakes (`FakeClock`, `FakeDigitalOutput`, `FakeDigitalInput`, `FakeConfigStore`) stand in for in domain tests. Nothing in this plan touches `test/support/`. (`CLAUDE.md`)
- **No dedicated native test per adapter.** Every prior class in this repo got a failing-test-first native TDD cycle; that's not available here because there is no native equivalent of `digitalWrite`/`Preferences`/`WiFi`/`PubSubClient`. Each adapter task instead ends with the build-check cycle described in Architecture above — do not skip it, and do not substitute a native test that fakes out the hardware call (that would just be testing the fake).
- **Adapters have zero automated coverage.** Combined with "any `if` in an adapter is a smell" (`docs/software-class-list.md`), keep every adapter's control flow to the minimum the design doc literally describes — do not add speculative behavior "while you're in there."
- `native`'s `build_flags` includes `-Ilib/McsCore/src`; adapters use the same include style as existing files (`"ports/Clock.h"`, `"domain/Instant.h"`, etc.) — no explicit `-I` needed for `esp32dev` since PlatformIO's LDF discovers `lib/McsCore` automatically once `src/main.cpp` includes something from it (confirmed empirically — nothing under `lib/McsCore` compiles today because nothing references it yet).
- **Commits go through `/arlo-commits`** (CLAUDE.md) — never hand-written `git commit`. ACN's "Small Features and Bug Fixes" rule caps any `F`/`B` commit over 8 lines at risk `!`. Every adapter task here is new `F` behavior with **zero automated test coverage**, which is itself a risk-elevating factor beyond just line count — flag this explicitly when invoking `/arlo-commits` so it isn't scored as if a passing test suite were backing it.
- **Commit often:** each task below is its own commit. Do not batch tasks.
- Every task ends with `pio test -e native` passing (25 binaries after Task 1's removal) — this is the regression guard that the `#ifdef ARDUINO` guards are actually keeping adapter code out of the native build.

---

## Task 1: Remove unused `PwmOutput` port and `FakePwmOutput`

`docs/software-class-list.md` already documents this as scaffolding predating the real design ("Tortoise stall motors are driven via simple direction-level `DigitalOutput`, not PWM speed control... a removal candidate rather than something to keep building on"). `docs/task-status.md`'s "Known scaffolding debt" section lists it as one of the two items this backlog step should resolve. Confirmed via `grep -rl PwmOutput` that only `lib/McsCore/src/ports/PwmOutput.h`, `test/support/FakePwmOutput.h`, `test/test_fake_pwm_output/test_main.cpp`, and doc files reference it — no domain/adapter class consumes it.

**Files:**
- Delete: `lib/McsCore/src/ports/PwmOutput.h`
- Delete: `test/support/FakePwmOutput.h`
- Delete: `test/test_fake_pwm_output/test_main.cpp` (and the now-empty `test/test_fake_pwm_output/` directory)
- Modify: `docs/task-status.md`

**Interfaces:** Consumes nothing, produces nothing — pure deletion.

- [x] **Step 1: Delete the three files**

```bash
git rm lib/McsCore/src/ports/PwmOutput.h
git rm test/support/FakePwmOutput.h
git rm test/test_fake_pwm_output/test_main.cpp
```

- [x] **Step 2: Run the native suite to confirm nothing broke**

Run: `pio test -e native`
Expected: PASS, 25 test binaries (one fewer than before — `test_fake_pwm_output` is gone).

- [x] **Step 3: Update `docs/task-status.md`**

In the "Known scaffolding debt" section, remove this bullet (the first one):

```markdown
- `PwmOutput` port (`lib/McsCore/src/ports/PwmOutput.h`) has no consumer
  anywhere in the design and is a removal candidate rather than something to
  keep building on.
```

- [x] **Step 4: Commit**

Use `/arlo-commits`. This is a deletion with a doc-accuracy fix — ACN intention letter `d`.

---

## Task 2: Fix `TopicScheme::parse`'s unguarded `std::stoi`

**Files:**
- Modify: `lib/McsCore/src/domain/TopicScheme.h`
- Modify: `test/test_topic_scheme/test_main.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: same signature, `static std::optional<TurnoutId> parse(const std::string& topic)` — now returns `std::nullopt` instead of throwing on an all-digit suffix too long for `int`. Task 9 (`MqttCommandSource`) is this fix's real-world caller, feeding it untrusted MQTT topic strings.

- [x] **Step 1: Write the failing test**

Add to `test/test_topic_scheme/test_main.cpp`:

```cpp
TEST_CASE("parse returns nullopt for a numeric suffix too large for int, instead of throwing")
{
    REQUIRE(TopicScheme::parse("track/turnout/99999999999") == std::nullopt);
}
```

- [x] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_topic_scheme`
Expected: FAIL — the test throws an uncaught `std::out_of_range` from `std::stoi` instead of returning cleanly (Catch2 reports it as a test failure due to the uncaught exception).

- [x] **Step 3: Fix `TopicScheme::parse`**

In `lib/McsCore/src/domain/TopicScheme.h`, replace:

```cpp
        return TurnoutId(std::stoi(suffix));
```

with:

```cpp
        try
        {
            return TurnoutId(std::stoi(suffix));
        }
        catch (const std::out_of_range&)
        {
            return std::nullopt;
        }
```

- [x] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_topic_scheme`
Expected: PASS.

- [x] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: PASS, 25 test binaries.

- [x] **Step 6: Commit**

Use `/arlo-commits`. Bug fix, ACN intention letter `B`.

---

## Task 3: `ArduinoClock` adapter

**Files:**
- Create: `lib/McsCore/src/adapters/ArduinoClock.h`

**Interfaces:**
- Consumes: `Clock` (`lib/McsCore/src/ports/Clock.h`), `Instant` (`lib/McsCore/src/domain/Instant.h`).
- Produces: `class ArduinoClock : public Clock` with `Instant now() const override`. Backlog #16 (`ControllerNode`) is the intended consumer.

- [x] **Step 1: Write `ArduinoClock`**

Create `lib/McsCore/src/adapters/ArduinoClock.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/Clock.h"
#include "domain/Instant.h"

// millis() wraps after ~49 days; per docs/software-class-list.md open item
// 10.6, deliberately unhandled here — a reboot happens well inside that
// window in practice, and Instant/Duration's unsigned-arithmetic comparisons
// already tolerate the single wrap-around case a reboot doesn't cover.
class ArduinoClock : public Clock
{
public:
    Instant now() const override
    {
        return Instant(millis());
    }
};

#endif
```

- [x] **Step 2: Temporarily wire it into `src/main.cpp` for a build check**

Replace `src/main.cpp`'s contents with:

```cpp
#include <Arduino.h>

#include "adapters/ArduinoClock.h"

void setup()
{
    ArduinoClock clock;
    (void)clock.now();
}

void loop()
{
}
```

- [x] **Step 3: Build for the real target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` (build completes, no compile/link errors).

- [x] **Step 4: Revert `src/main.cpp`**

```bash
git checkout -- src/main.cpp
```

`src/main.cpp` must go back to its no-op stub — wiring it for real is backlog #16, not this task.

- [x] **Step 5: Confirm native is unaffected**

Run: `pio test -e native`
Expected: PASS, 25 test binaries (native never sees `adapters/ArduinoClock.h` — it's `#ifdef ARDUINO`-guarded and nothing in `test/` includes it).

- [x] **Step 6: Commit**

Use `/arlo-commits`. New adapter, zero automated coverage — ACN intention letter `F`, flag the no-coverage caveat from Global Constraints.

---

## Task 4: `EspDigitalOutput` adapter

**Files:**
- Create: `lib/McsCore/src/adapters/EspDigitalOutput.h`

**Interfaces:**
- Consumes: `DigitalOutput` (`lib/McsCore/src/ports/DigitalOutput.h`), `Level` (`lib/McsCore/src/domain/Level.h`).
- Produces: `class EspDigitalOutput : public DigitalOutput` with `explicit EspDigitalOutput(int pin)`, `void write(Level level) override`. Backlog #16 is the intended consumer (one per `Turnout`'s output pin).

- [x] **Step 1: Write `EspDigitalOutput`**

Create `lib/McsCore/src/adapters/EspDigitalOutput.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/DigitalOutput.h"
#include "domain/Level.h"

class EspDigitalOutput : public DigitalOutput
{
public:
    explicit EspDigitalOutput(int pin) : pin_(pin)
    {
        pinMode(pin_, OUTPUT);
    }

    void write(Level level) override
    {
        digitalWrite(pin_, level == Level::High ? HIGH : LOW);
    }

private:
    int pin_;
};

#endif
```

- [x] **Step 2: Temporarily wire it into `src/main.cpp` for a build check**

Replace `src/main.cpp`'s contents with:

```cpp
#include <Arduino.h>

#include "adapters/EspDigitalOutput.h"

void setup()
{
    EspDigitalOutput output(2);
    output.write(Level::High);
}

void loop()
{
}
```

- [x] **Step 3: Build for the real target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [x] **Step 4: Revert `src/main.cpp`**

```bash
git checkout -- src/main.cpp
```

- [x] **Step 5: Confirm native is unaffected**

Run: `pio test -e native`
Expected: PASS, 25 test binaries.

- [x] **Step 6: Commit**

Use `/arlo-commits`. ACN intention letter `F`, no-coverage caveat.

---

## Task 5: `EspDigitalInput` adapter

**Files:**
- Create: `lib/McsCore/src/adapters/EspDigitalInput.h`

**Interfaces:**
- Consumes: `DigitalInput` (`lib/McsCore/src/ports/DigitalInput.h`), `Level`.
- Produces: `class EspDigitalInput : public DigitalInput` with `EspDigitalInput(int pin, bool hasInternalPullUp)`, `Level read() override`. `hasInternalPullUp` matters because GPIO 36/39 on the ESP32-WROOM-32 have no internal pull-up (`docs/software-class-list.md`). Backlog #16 is the intended consumer (one per `Turnout`'s feedback pin).

- [x] **Step 1: Write `EspDigitalInput`**

Create `lib/McsCore/src/adapters/EspDigitalInput.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/DigitalInput.h"
#include "domain/Level.h"

class EspDigitalInput : public DigitalInput
{
public:
    EspDigitalInput(int pin, bool hasInternalPullUp) : pin_(pin)
    {
        pinMode(pin_, hasInternalPullUp ? INPUT_PULLUP : INPUT);
    }

    Level read() override
    {
        return digitalRead(pin_) == HIGH ? Level::High : Level::Low;
    }

private:
    int pin_;
};

#endif
```

- [x] **Step 2: Temporarily wire it into `src/main.cpp` for a build check**

Replace `src/main.cpp`'s contents with:

```cpp
#include <Arduino.h>

#include "adapters/EspDigitalInput.h"

void setup()
{
    EspDigitalInput input(36, false);
    (void)input.read();
}

void loop()
{
}
```

- [x] **Step 3: Build for the real target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [x] **Step 4: Revert `src/main.cpp`**

```bash
git checkout -- src/main.cpp
```

- [x] **Step 5: Confirm native is unaffected**

Run: `pio test -e native`
Expected: PASS, 25 test binaries.

- [x] **Step 6: Commit**

Use `/arlo-commits`. ACN intention letter `F`, no-coverage caveat.

---

## Task 6: `NvsConfigStore` adapter

**Files:**
- Create: `lib/McsCore/src/adapters/NvsConfigStore.h`

**Interfaces:**
- Consumes: `ConfigStore` (`lib/McsCore/src/ports/ConfigStore.h`), `NodeConfig`/`ConfigError` (`lib/McsCore/src/domain/NodeConfig.h`), `NodeId`, `WifiCredentials`, `BrokerAddress`, `TurnoutConfig`, `TurnoutId`, `Orientation`, `TurnoutPosition`, `Level`, `Duration`.
- Produces: `class NvsConfigStore : public ConfigStore` with `void save(const NodeConfig&) override`, `NodeConfig load() override`. Backlog #16 and #18 (bench serial commissioning) are the intended consumers — `load()` is what a fresh boot calls, `save()` is what `CommissioningSession::save()` will eventually call.

`Orientation` (`lib/McsCore/src/domain/Orientation.h`) exposes no raw `inverted` accessor — only `toLevel(TurnoutPosition)`/`toPosition(Level)`. `TurnoutConfig::operator==` (`lib/McsCore/src/domain/TurnoutConfig.h:63`) already distinguishes the two `Orientation` states via `orientation.toLevel(TurnoutPosition::closed())`; this task reuses that exact probe to turn an `Orientation` into a storable `bool` and back.

ESP32 NVS (`Preferences`) namespace and key names are capped at 15 characters — every key below stays well under that.

- [x] **Step 1: Write `NvsConfigStore`**

Create `lib/McsCore/src/adapters/NvsConfigStore.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <Preferences.h>

#include <string>

#include "ports/ConfigStore.h"
#include "domain/NodeConfig.h"
#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutConfig.h"
#include "domain/TurnoutId.h"
#include "domain/Orientation.h"
#include "domain/TurnoutPosition.h"
#include "domain/Level.h"
#include "domain/Duration.h"

class NvsConfigStore : public ConfigStore
{
public:
    void save(const NodeConfig& config) override
    {
        Preferences prefs;
        prefs.begin(kNamespace, false);

        prefs.putInt("id", config.id().value());
        prefs.putString("wssid", config.wifi().ssid().c_str());
        prefs.putString("wpass", config.wifi().password().c_str());
        prefs.putString("bhost", config.broker().host().c_str());
        prefs.putInt("bport", config.broker().port());

        for (int i = 0; i < 8; ++i)
        {
            const TurnoutConfig& turnout = config.turnouts()[i];
            std::string prefix = "t" + std::to_string(i);
            prefs.putInt((prefix + "op").c_str(), turnout.outputPin());
            prefs.putInt((prefix + "fp").c_str(), turnout.feedbackPin());
            prefs.putBool((prefix + "iv").c_str(), isInverted(turnout.orientation()));
            prefs.putULong((prefix + "se").c_str(), turnout.settleDuration().milliseconds());
            prefs.putULong((prefix + "mt").c_str(), turnout.movementTimeout().milliseconds());
        }

        prefs.end();
    }

    NodeConfig load() override
    {
        Preferences prefs;
        prefs.begin(kNamespace, true);

        NodeConfig config = NodeConfig::factoryDefault();

        config = config.withId(NodeId(prefs.getInt("id", config.id().value())));
        config = config.withWifi(WifiCredentials(
            prefs.getString("wssid", config.wifi().ssid().c_str()).c_str(),
            prefs.getString("wpass", config.wifi().password().c_str()).c_str()));
        config = config.withBroker(BrokerAddress(
            prefs.getString("bhost", config.broker().host().c_str()).c_str(),
            prefs.getInt("bport", config.broker().port())));

        for (int i = 0; i < 8; ++i)
        {
            const TurnoutConfig& fallback = config.turnouts()[i];
            std::string prefix = "t" + std::to_string(i);
            int outputPin = prefs.getInt((prefix + "op").c_str(), fallback.outputPin());
            int feedbackPin = prefs.getInt((prefix + "fp").c_str(), fallback.feedbackPin());
            bool inverted = prefs.getBool((prefix + "iv").c_str(), isInverted(fallback.orientation()));
            unsigned long settle = prefs.getULong((prefix + "se").c_str(), fallback.settleDuration().milliseconds());
            unsigned long timeout = prefs.getULong((prefix + "mt").c_str(), fallback.movementTimeout().milliseconds());

            config = config.withTurnout(i, TurnoutConfig(
                fallback.id(),
                outputPin,
                feedbackPin,
                inverted ? Orientation::inverted() : Orientation::normal(),
                Duration(settle),
                Duration(timeout)));
        }

        prefs.end();
        return config;
    }

private:
    static constexpr const char* kNamespace = "mcs-cfg";

    static bool isInverted(Orientation orientation)
    {
        return orientation.toLevel(TurnoutPosition::closed()) == Level::High;
    }
};

#endif
```

- [x] **Step 2: Temporarily wire it into `src/main.cpp` for a build check**

Replace `src/main.cpp`'s contents with:

```cpp
#include <Arduino.h>

#include "adapters/NvsConfigStore.h"

void setup()
{
    NvsConfigStore store;
    store.save(NodeConfig::factoryDefault());
    NodeConfig loaded = store.load();
    (void)loaded;
}

void loop()
{
}
```

- [x] **Step 3: Build for the real target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [x] **Step 4: Revert `src/main.cpp`**

```bash
git checkout -- src/main.cpp
```

- [x] **Step 5: Confirm native is unaffected**

Run: `pio test -e native`
Expected: PASS, 25 test binaries.

- [x] **Step 6: Commit**

Use `/arlo-commits`. ACN intention letter `F`, no-coverage caveat.

---

## Task 7: `WiFiLink` adapter

**Files:**
- Create: `lib/McsCore/src/adapters/WiFiLink.h`

**Interfaces:**
- Consumes: `Clock`/`Instant` (for non-blocking retry pacing), `Duration`, `WifiCredentials`.
- Produces: `class WiFiLink` (no port — matches `docs/software-class-list.md`'s Adapters table, which lists `WiFiLink` without a corresponding Ports-table entry) with `WiFiLink(Clock& clock, Duration retryInterval)`, `void begin(const WifiCredentials& credentials)`, `void poll()`, `bool connected() const`. Backlog #16 is the intended consumer, calling `poll()` once per `ControllerNode::tick()`.

`WiFi.begin()` itself is non-blocking, but calling it on *every* `poll()` while disconnected would re-trigger association on every single un-delayed `loop()` iteration — since `CLAUDE.md` forbids `delay()` anywhere, `tick()`/`poll()` will be called continuously, so `poll()` needs a real elapsed-time gate, not just a non-blocking call. This is why `WiFiLink` takes a `Clock&` and a retry `Duration`, following the same DI shape as `Deadline`.

- [x] **Step 1: Write `WiFiLink`**

Create `lib/McsCore/src/adapters/WiFiLink.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <WiFi.h>

#include <string>

#include "ports/Clock.h"
#include "domain/Instant.h"
#include "domain/Duration.h"
#include "domain/WifiCredentials.h"

class WiFiLink
{
public:
    WiFiLink(Clock& clock, Duration retryInterval)
        : clock_(clock), retryInterval_(retryInterval), lastAttempt_(Instant(0))
    {
    }

    void begin(const WifiCredentials& credentials)
    {
        ssid_ = credentials.ssid();
        password_ = credentials.password();
        connect();
    }

    void poll()
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            return;
        }

        if (clock_.now() - lastAttempt_ >= retryInterval_)
        {
            connect();
        }
    }

    bool connected() const
    {
        return WiFi.status() == WL_CONNECTED;
    }

private:
    void connect()
    {
        WiFi.begin(ssid_.c_str(), password_.c_str());
        lastAttempt_ = clock_.now();
    }

    Clock& clock_;
    Duration retryInterval_;
    Instant lastAttempt_;
    std::string ssid_;
    std::string password_;
};

#endif
```

- [x] **Step 2: Temporarily wire it into `src/main.cpp` for a build check**

Replace `src/main.cpp`'s contents with:

```cpp
#include <Arduino.h>

#include "adapters/ArduinoClock.h"
#include "adapters/WiFiLink.h"

void setup()
{
    ArduinoClock clock;
    WiFiLink link(clock, Duration(5000));
    link.begin(WifiCredentials("ssid", "password"));
    link.poll();
    (void)link.connected();
}

void loop()
{
}
```

- [x] **Step 3: Build for the real target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [x] **Step 4: Revert `src/main.cpp`**

```bash
git checkout -- src/main.cpp
```

- [x] **Step 5: Confirm native is unaffected**

Run: `pio test -e native`
Expected: PASS, 25 test binaries.

- [x] **Step 6: Commit**

Use `/arlo-commits`. ACN intention letter `F`, no-coverage caveat.

---

## Task 8: `MqttLink` adapter

**Files:**
- Create: `lib/McsCore/src/adapters/MqttLink.h`

**Interfaces:**
- Consumes: `Clock`/`Instant`, `Duration`, `BrokerAddress`.
- Produces: `class MqttLink` (no port, same reasoning as `WiFiLink`) with `MqttLink(Clock& clock, Duration retryInterval, std::string clientId, std::string willTopic, std::string willMessage)`, `void begin(const BrokerAddress& broker)`, `void poll()`, `bool connected()` (not `const` — `PubSubClient::connected()` isn't const-qualified, caught by the `esp32dev` build check below), `PubSubClient& raw()`. Tasks 9 (`MqttCommandSource`) and 10 (`MqttPositionReporter`) both depend on `raw()` to call `subscribe`/`publish`/`setCallback` directly — `MqttLink` owns only connection lifecycle (matching its one-line description in `docs/software-class-list.md`: "broker connection... non-blocking reconnect... sets a Last Will and Testament"), not every pub/sub method PubSubClient exposes.

Same non-blocking-reconnect reasoning as `WiFiLink` (Task 7) applies here — `poll()` gates reconnect attempts on `retryInterval` via the injected `Clock`, not on every call.

`clientId`/`willTopic`/`willMessage` are taken by value as `std::string` (not `const char*`) so `MqttLink` owns its own copies for the object's full lifetime — a caller passing a temporary `const char*` would otherwise dangle.

- [x] **Step 1: Write `MqttLink`**

Create `lib/McsCore/src/adapters/MqttLink.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <WiFiClient.h>
#include <PubSubClient.h>

#include <string>
#include <utility>

#include "ports/Clock.h"
#include "domain/Instant.h"
#include "domain/Duration.h"
#include "domain/BrokerAddress.h"

class MqttLink
{
public:
    MqttLink(Clock& clock, Duration retryInterval, std::string clientId, std::string willTopic, std::string willMessage)
        : clock_(clock),
          retryInterval_(retryInterval),
          clientId_(std::move(clientId)),
          willTopic_(std::move(willTopic)),
          willMessage_(std::move(willMessage)),
          client_(wifiClient_),
          lastAttempt_(Instant(0))
    {
    }

    void begin(const BrokerAddress& broker)
    {
        client_.setServer(broker.host().c_str(), broker.port());
        connect();
    }

    void poll()
    {
        if (client_.connected())
        {
            client_.loop();
            return;
        }

        if (clock_.now() - lastAttempt_ >= retryInterval_)
        {
            connect();
        }
    }

    // Not const: PubSubClient::connected() isn't const-qualified.
    bool connected()
    {
        return client_.connected();
    }

    PubSubClient& raw()
    {
        return client_;
    }

private:
    void connect()
    {
        client_.connect(clientId_.c_str(), willTopic_.c_str(), 1, true, willMessage_.c_str());
        lastAttempt_ = clock_.now();
    }

    Clock& clock_;
    Duration retryInterval_;
    std::string clientId_;
    std::string willTopic_;
    std::string willMessage_;
    WiFiClient wifiClient_;
    PubSubClient client_;
    Instant lastAttempt_;
};

#endif
```

- [x] **Step 2: Temporarily wire it into `src/main.cpp` for a build check**

Replace `src/main.cpp`'s contents with:

```cpp
#include <Arduino.h>

#include "adapters/ArduinoClock.h"
#include "adapters/MqttLink.h"

void setup()
{
    ArduinoClock clock;
    MqttLink link(clock, Duration(5000), "node1", "node/1/status", "offline");
    link.begin(BrokerAddress("broker.local", 1883));
    link.poll();
    (void)link.connected();
}

void loop()
{
}
```

- [x] **Step 3: Build for the real target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [x] **Step 4: Revert `src/main.cpp`**

```bash
git checkout -- src/main.cpp
```

- [x] **Step 5: Confirm native is unaffected**

Run: `pio test -e native`
Expected: PASS, 25 test binaries.

- [x] **Step 6: Commit**

Use `/arlo-commits`. ACN intention letter `F`, no-coverage caveat.

---

## Task 9: `MqttCommandSource` adapter

**Files:**
- Create: `lib/McsCore/src/adapters/MqttCommandSource.h`

**Interfaces:**
- Consumes: `MqttLink` (Task 8, specifically `raw()`), `TopicScheme::parse` (Task 2's fixed version), `PayloadCodec::decode`, `TurnoutCommandSink`, `TurnoutRegistry::TurnoutsPerNode` (`lib/McsCore/src/domain/TurnoutRegistry.h:16`).
- Produces: `class MqttCommandSource` with `MqttCommandSource(MqttLink& link, TurnoutCommandSink& sink)`, `void subscribeAll(int nodeId)`. Backlog #16 is the intended consumer.

**Confirmed against the actual vendored library** (`.pio/libdeps/esp32dev/PubSubClient/src/PubSubClient.h:79-84`): on `ESP32`, `MQTT_CALLBACK_SIGNATURE` is `std::function<void(char*, uint8_t*, unsigned int)>`, not a plain C function pointer — so `setCallback` accepts a capturing lambda directly. No static instance pointer or trampoline is needed; the constructor just passes a lambda that captures `this` and forwards to the private `handle` method.

- [ ] **Step 1: Write `MqttCommandSource`**

Create `lib/McsCore/src/adapters/MqttCommandSource.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <PubSubClient.h>

#include <optional>
#include <string>

#include "adapters/MqttLink.h"
#include "domain/TopicScheme.h"
#include "domain/PayloadCodec.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutRegistry.h"
#include "ports/TurnoutCommandSink.h"

class MqttCommandSource
{
public:
    MqttCommandSource(MqttLink& link, TurnoutCommandSink& sink)
        : link_(link), sink_(sink)
    {
        link_.raw().setCallback([this](char* topic, byte* payload, unsigned int length) {
            handle(topic, payload, length);
        });
    }

    void subscribeAll(int nodeId)
    {
        for (int channel = 1; channel <= TurnoutRegistry::TurnoutsPerNode; ++channel)
        {
            std::string topic = TopicScheme::topicFor(TurnoutId(nodeId * 100 + channel));
            link_.raw().subscribe(topic.c_str());
        }
    }

private:
    void handle(char* topic, byte* payload, unsigned int length)
    {
        std::optional<TurnoutId> id = TopicScheme::parse(topic);
        if (!id.has_value())
        {
            return;
        }

        std::string text(reinterpret_cast<char*>(payload), length);
        std::optional<TurnoutPosition> position = PayloadCodec::decode(text);
        if (!position.has_value())
        {
            return;
        }

        sink_.command(*id, *position);
    }

    MqttLink& link_;
    TurnoutCommandSink& sink_;
};

#endif
```

- [ ] **Step 2: Temporarily wire it into `src/main.cpp` for a build check**

Replace `src/main.cpp`'s contents with:

```cpp
#include <Arduino.h>

#include "adapters/ArduinoClock.h"
#include "adapters/MqttLink.h"
#include "adapters/MqttCommandSource.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "ports/TurnoutCommandSink.h"

class NullSink : public TurnoutCommandSink
{
public:
    void command(TurnoutId, TurnoutPosition) override
    {
    }
};

void setup()
{
    ArduinoClock clock;
    MqttLink link(clock, Duration(5000), "node1", "node/1/status", "offline");
    NullSink sink;
    MqttCommandSource source(link, sink);
    source.subscribeAll(1);
}

void loop()
{
}
```

- [ ] **Step 3: Build for the real target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 4: Revert `src/main.cpp`**

```bash
git checkout -- src/main.cpp
```

- [ ] **Step 5: Confirm native is unaffected**

Run: `pio test -e native`
Expected: PASS, 25 test binaries.

- [ ] **Step 6: Commit**

Use `/arlo-commits`. ACN intention letter `F`, no-coverage caveat.

---

## Task 10: `MqttPositionReporter` adapter

**Files:**
- Create: `lib/McsCore/src/adapters/MqttPositionReporter.h`

**Interfaces:**
- Consumes: `MqttLink` (Task 8, `raw()`), `PositionReporter`, `TopicScheme::topicFor`, `PayloadCodec::encode(TurnoutState)`.
- Produces: `class MqttPositionReporter : public PositionReporter` with `explicit MqttPositionReporter(MqttLink& link)`, `void report(TurnoutId id, TurnoutState state) override`. Backlog #16 is the intended consumer (one instance shared by every `Turnout`, per `docs/software-class-list.md`'s "Why the retain flag matters" — an unretained report published before JMRI connects would be lost).

- [ ] **Step 1: Write `MqttPositionReporter`**

Create `lib/McsCore/src/adapters/MqttPositionReporter.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <string>

#include "adapters/MqttLink.h"
#include "ports/PositionReporter.h"
#include "domain/TopicScheme.h"
#include "domain/PayloadCodec.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutState.h"

class MqttPositionReporter : public PositionReporter
{
public:
    explicit MqttPositionReporter(MqttLink& link) : link_(link)
    {
    }

    void report(TurnoutId id, TurnoutState state) override
    {
        std::string topic = TopicScheme::topicFor(id);
        std::string payload = PayloadCodec::encode(state);
        link_.raw().publish(topic.c_str(), payload.c_str(), true);
    }

private:
    MqttLink& link_;
};

#endif
```

- [ ] **Step 2: Temporarily wire it into `src/main.cpp` for a build check**

Replace `src/main.cpp`'s contents with:

```cpp
#include <Arduino.h>

#include "adapters/ArduinoClock.h"
#include "adapters/MqttLink.h"
#include "adapters/MqttPositionReporter.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutState.h"

void setup()
{
    ArduinoClock clock;
    MqttLink link(clock, Duration(5000), "node1", "node/1/status", "offline");
    MqttPositionReporter reporter(link);
    reporter.report(TurnoutId(101), TurnoutState::Closed);
}

void loop()
{
}
```

- [ ] **Step 3: Build for the real target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 4: Revert `src/main.cpp`**

```bash
git checkout -- src/main.cpp
```

- [ ] **Step 5: Confirm native is unaffected**

Run: `pio test -e native`
Expected: PASS, 25 test binaries.

- [ ] **Step 6: Commit**

Use `/arlo-commits`. ACN intention letter `F`, no-coverage caveat.

---

## After this plan

`src/main.cpp` is still the no-op stub at the end of Task 10 — every task reverts it deliberately. Backlog #16 (`ControllerNode` + `main.cpp` composition root) is what actually wires these eight adapters into a running board, and is the next plan to write. Once this plan's branch is merged, update `docs/task-status.md` to mark backlog #15 complete (following the pattern of commit `3f49143`, done as its own commit after merge, not as part of any task above).
