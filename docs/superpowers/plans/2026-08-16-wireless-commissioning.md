# Wireless Commissioning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the domain/adapter classes for backlog #19 (Wireless commissioning) so a non-technical customer can configure a factory-default board over its own WiFi access point instead of a serial terminal — reusing the existing `ParsedCommand`/`CommandLineParser`/`CommissioningSession` classes built for bench serial commissioning (backlog #18).

**Architecture:** Hexagonal, same discipline as the rest of this repo (see `CLAUDE.md`). New ports: `SetupModeTrigger`, `DeviceIdentity`. New pure domain classes: `MacAddress`, `SetupApName`, `WebFormSubmission`. New adapters: `ButtonSetupModeTrigger`, `WebFormCommissioningAdapter`, `EspDeviceIdentity`, `CaptivePortalServer`.

**Tech Stack:** PlatformIO, Catch2 3.7.1 (native tests), Arduino framework for `esp32dev` (WiFi/DNSServer/WebServer are built into the `espressif32` Arduino core — no new `lib_deps` needed).

## Global Constraints

- Domain/application code must compile and run under the `native` PlatformIO environment without `Arduino.h`. Only `EspDeviceIdentity` and `CaptivePortalServer` may include Arduino/ESP32 headers, and only inside `#ifdef ARDUINO`.
- No `delay()` anywhere in domain/application code. Non-blocking `poll(Instant now)` methods only (mirrors `FeedbackSensor::sample(Instant now)`).
- No dynamic allocation after boot in adapters that run in the hot loop (not a concern for this plan's classes — they're either boot-time-only or pure/native-tested).
- ACN notation for every commit message (`references/acn-notation.md` under the `arlo-commits` skill has the full spec). Never `--amend`, never `--no-verify`.
- **Deliberate architectural deviation (apply to two classes in this plan):** `ButtonSetupModeTrigger` and `WebFormCommissioningAdapter` are classified "Adapter" in `docs/software-class-list.md`, but — like `SerialCommissioningAdapter` before them (backlog #18) — they depend only on pure ports/domain classes (`DigitalInput`, `Duration`, `Instant`, `ParsedCommand`, `CommissioningSession`, `CommandLineParser`), never on `Arduino.h` directly. Give them **no** `#ifdef ARDUINO` guard, and give them full native TDD coverage. This is intentional — do not "fix" it to match `EspDeviceIdentity`/`CaptivePortalServer`'s guard style, and do not flag it as an inconsistency in review.
- This plan does **not** wire any of these classes into `ControllerNode`/`src/main.cpp`. That mirrors the established split from backlog #15→#16 and #17→#18: build and verify the classes now, wire them into the composition root in a later, separate task once boot-mode-selection logic (deciding when to run setup mode vs. normal operation) actually exists. Do not add that wiring in this plan.
- `EspDeviceIdentity` and `CaptivePortalServer` have no native equivalent (real WiFi/DNS/HTTP hardware). Verify them with the **build-check cycle** established in backlog #15/#16/#18: temporarily wire the class into `src/main.cpp`, run `pio run -e esp32dev`, then revert `main.cpp` to its exact original content and rebuild to confirm the revert is clean. Confirm via `git diff src/main.cpp` that it shows zero output right after the revert, before committing.

---

### Task 1: `MacAddress` value object

**Files:**
- Create: `lib/McsCore/src/domain/MacAddress.h`
- Test: `test/test_mac_address/test_main.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `MacAddress` — constructed from 6 raw bytes; `lastFourHexDigits()` used by Task 2's `SetupApName`.

A read-only wrapper around a 6-byte hardware MAC address. Only real need right now: deriving the last 4 hex digits for the setup-AP name (e.g. `Tortoise-Setup-3F2A`).

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_mac_address/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/MacAddress.h"

TEST_CASE("MacAddress stores and returns the 6 raw bytes")
{
    MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0x3F, 0x2A});

    std::array<uint8_t, 6> bytes = mac.bytes();
    REQUIRE(bytes[0] == 0x24);
    REQUIRE(bytes[5] == 0x2A);
}

TEST_CASE("MacAddress::lastFourHexDigits formats the last two bytes as uppercase hex")
{
    MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0x3F, 0x2A});
    REQUIRE(mac.lastFourHexDigits() == "3F2A");
}

TEST_CASE("MacAddress::lastFourHexDigits zero-pads single-hex-digit bytes")
{
    MacAddress mac({0x00, 0x00, 0x00, 0x00, 0x00, 0x0A});
    REQUIRE(mac.lastFourHexDigits() == "000A");
}

TEST_CASE("MacAddress equality compares all 6 bytes")
{
    MacAddress a({1, 2, 3, 4, 5, 6});
    MacAddress b({1, 2, 3, 4, 5, 6});
    MacAddress c({1, 2, 3, 4, 5, 7});

    REQUIRE(a == b);
    REQUIRE(a != c);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_mac_address`
Expected: FAIL to compile — `domain/MacAddress.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/McsCore/src/domain/MacAddress.h
#pragma once

#include <array>
#include <cstdint>
#include <string>

class MacAddress
{
public:
    explicit MacAddress(std::array<uint8_t, 6> bytes) : bytes_(bytes)
    {
    }

    const std::array<uint8_t, 6>& bytes() const
    {
        return bytes_;
    }

    std::string lastFourHexDigits() const
    {
        static const char* kHexDigits = "0123456789ABCDEF";
        std::string result;
        result += kHexDigits[(bytes_[4] >> 4) & 0x0F];
        result += kHexDigits[bytes_[4] & 0x0F];
        result += kHexDigits[(bytes_[5] >> 4) & 0x0F];
        result += kHexDigits[bytes_[5] & 0x0F];
        return result;
    }

    bool operator==(const MacAddress& other) const
    {
        return bytes_ == other.bytes_;
    }

    bool operator!=(const MacAddress& other) const
    {
        return !(*this == other);
    }

private:
    std::array<uint8_t, 6> bytes_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_mac_address`
Expected: PASS, 4 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/domain/MacAddress.h test/test_mac_address/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add MacAddress value object

EOF
)"
```

---

### Task 2: `SetupApName` formatter

**Files:**
- Create: `lib/McsCore/src/domain/SetupApName.h`
- Test: `test/test_setup_ap_name/test_main.cpp`

**Interfaces:**
- Consumes: `MacAddress` (Task 1) — `lastFourHexDigits()`.
- Produces: `SetupApName::from(const MacAddress&) -> std::string`, used by Task 6's `CaptivePortalServer`.

Pure static formatter, same style as `TopicScheme`/`PayloadCodec`. Produces the setup-mode AP name: `Tortoise-Setup-<last 4 hex digits of MAC>`.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_setup_ap_name/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/SetupApName.h"
#include "domain/MacAddress.h"

TEST_CASE("SetupApName::from formats Tortoise-Setup-<last 4 hex digits>")
{
    MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0x3F, 0x2A});
    REQUIRE(SetupApName::from(mac) == "Tortoise-Setup-3F2A");
}

TEST_CASE("SetupApName::from zero-pads short hex digits")
{
    MacAddress mac({0x00, 0x00, 0x00, 0x00, 0x00, 0x0A});
    REQUIRE(SetupApName::from(mac) == "Tortoise-Setup-000A");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_setup_ap_name`
Expected: FAIL to compile — `domain/SetupApName.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/McsCore/src/domain/SetupApName.h
#pragma once

#include <string>

#include "domain/MacAddress.h"

class SetupApName
{
public:
    static std::string from(const MacAddress& mac)
    {
        return "Tortoise-Setup-" + mac.lastFourHexDigits();
    }
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_setup_ap_name`
Expected: PASS, 2 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/domain/SetupApName.h test/test_setup_ap_name/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add SetupApName formatter

EOF
)"
```

---

### Task 3: `SetupModeTrigger`/`DeviceIdentity` ports and fakes

**Files:**
- Create: `lib/McsCore/src/ports/SetupModeTrigger.h`
- Create: `lib/McsCore/src/ports/DeviceIdentity.h`
- Create: `test/support/FakeSetupModeTrigger.h`
- Create: `test/support/FakeDeviceIdentity.h`
- Test: `test/test_setup_mode_trigger_fakes/test_main.cpp`

**Interfaces:**
- Consumes: `MacAddress` (Task 1) for `DeviceIdentity::mac()`.
- Produces: `SetupModeTrigger` (consumed by Task 4's `ButtonSetupModeTrigger`), `DeviceIdentity` (consumed by Task 6's `EspDeviceIdentity`).

Two tiny one-method ports, bundled into one task like `UartPort`/`FakeUartPort` was in backlog #18 — both trivial, both reviewed together.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_setup_mode_trigger_fakes/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeSetupModeTrigger.h"
#include "support/FakeDeviceIdentity.h"

TEST_CASE("FakeSetupModeTrigger defaults to not requested")
{
    FakeSetupModeTrigger trigger;
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("FakeSetupModeTrigger reports whatever was set")
{
    FakeSetupModeTrigger trigger;
    trigger.setRequested(true);
    REQUIRE(trigger.requested());
}

TEST_CASE("FakeDeviceIdentity reports the configured MAC")
{
    MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0x3F, 0x2A});
    FakeDeviceIdentity identity(mac);
    REQUIRE(identity.mac() == mac);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_setup_mode_trigger_fakes`
Expected: FAIL to compile — none of the new headers exist yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/McsCore/src/ports/SetupModeTrigger.h
#pragma once

class SetupModeTrigger
{
public:
    virtual ~SetupModeTrigger() = default;
    virtual bool requested() const = 0;
};
```

```cpp
// lib/McsCore/src/ports/DeviceIdentity.h
#pragma once

#include "domain/MacAddress.h"

class DeviceIdentity
{
public:
    virtual ~DeviceIdentity() = default;
    virtual MacAddress mac() const = 0;
};
```

```cpp
// test/support/FakeSetupModeTrigger.h
#pragma once

#include "ports/SetupModeTrigger.h"

class FakeSetupModeTrigger : public SetupModeTrigger
{
public:
    void setRequested(bool requested)
    {
        requested_ = requested;
    }

    bool requested() const override
    {
        return requested_;
    }

private:
    bool requested_ = false;
};
```

```cpp
// test/support/FakeDeviceIdentity.h
#pragma once

#include "ports/DeviceIdentity.h"

class FakeDeviceIdentity : public DeviceIdentity
{
public:
    explicit FakeDeviceIdentity(MacAddress mac) : mac_(mac)
    {
    }

    MacAddress mac() const override
    {
        return mac_;
    }

private:
    MacAddress mac_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_setup_mode_trigger_fakes`
Expected: PASS, 3 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/ports/SetupModeTrigger.h lib/McsCore/src/ports/DeviceIdentity.h \
        test/support/FakeSetupModeTrigger.h test/support/FakeDeviceIdentity.h \
        test/test_setup_mode_trigger_fakes/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add SetupModeTrigger and DeviceIdentity ports

EOF
)"
```

---

### Task 4: `ButtonSetupModeTrigger` (native-tested, deliberate deviation)

**Files:**
- Create: `lib/McsCore/src/adapters/ButtonSetupModeTrigger.h`
- Test: `test/test_button_setup_mode_trigger/test_main.cpp`

**Interfaces:**
- Consumes: `SetupModeTrigger` (Task 3, base class), `DigitalInput` port (existing, `lib/McsCore/src/ports/DigitalInput.h` — `Level read()`), `FakeDigitalInput` (existing, `test/support/FakeDigitalInput.h` — `enqueue(Level)`), `Duration`/`Instant`/`Level` (existing).
- Produces: `ButtonSetupModeTrigger` — not consumed by any other task in this plan (composition-root wiring is deferred, per Global Constraints).

**This is the deliberate architectural deviation called out in Global Constraints** — it lives in `adapters/` (matching `docs/software-class-list.md`'s layer classification) but has **no** `#ifdef ARDUINO` guard, because it depends only on the pure `DigitalInput` port and pure domain value objects. Reads the BOOT pin (active-low: pressed = `Level::Low`) and determines "was BOOT held continuously through the entire boot window?" via a non-blocking `poll(Instant now)` method, mirroring `FeedbackSensor::sample(Instant now)` — no `delay()`, no busy-waiting. A caller (the composition root, in a later task) calls `poll()` repeatedly with real clock readings during the boot window before starting normal operation.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_button_setup_mode_trigger/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/ButtonSetupModeTrigger.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("ButtonSetupModeTrigger is not requested before the boot window elapses")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(2000));

    trigger.poll(Instant(0));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger is requested when BOOT stays low through the whole window")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(1000));
    trigger.poll(Instant(2000));

    REQUIRE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger is never requested if BOOT goes high at any point")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(1000));
    trigger.poll(Instant(2000));

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger requires the full window even if BOOT is held longer")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(2000));

    trigger.poll(Instant(500));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger stays requested once the window has elapsed, across later polls")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(2000));
    REQUIRE(trigger.requested());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_button_setup_mode_trigger`
Expected: FAIL to compile — `adapters/ButtonSetupModeTrigger.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/McsCore/src/adapters/ButtonSetupModeTrigger.h
#pragma once

#include "ports/SetupModeTrigger.h"
#include "ports/DigitalInput.h"
#include "domain/Level.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

class ButtonSetupModeTrigger : public SetupModeTrigger
{
public:
    ButtonSetupModeTrigger(DigitalInput& bootPin, Duration bootWindow)
        : bootPin_(bootPin), bootWindow_(bootWindow)
    {
    }

    // Call repeatedly with the current time during the boot window, before
    // normal operation starts. Non-blocking - no delay().
    void poll(Instant now)
    {
        if (firstSample_)
        {
            windowStart_ = now;
            firstSample_ = false;
        }

        if (bootPin_.read() != Level::Low)
        {
            heldThroughout_ = false;
        }

        elapsedSinceWindowStart_ = now - windowStart_;
    }

    bool requested() const override
    {
        return heldThroughout_ && elapsedSinceWindowStart_ >= bootWindow_;
    }

private:
    DigitalInput& bootPin_;
    Duration bootWindow_;
    bool firstSample_ = true;
    Instant windowStart_ = Instant(0);
    Duration elapsedSinceWindowStart_ = Duration(0);
    bool heldThroughout_ = true;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_button_setup_mode_trigger`
Expected: PASS, 5 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/adapters/ButtonSetupModeTrigger.h test/test_button_setup_mode_trigger/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add ButtonSetupModeTrigger (native-tested, no ARDUINO guard)

Deliberate deviation from the adapter/ARDUINO-guard convention, same
rationale as SerialCommissioningAdapter (backlog #18): depends only on
the pure DigitalInput port and domain value objects, so it earns full
native TDD coverage instead of build-check-only verification.

EOF
)"
```

---

### Task 5: `WebFormSubmission` + `WebFormCommissioningAdapter` (native-tested, deliberate deviation)

**Files:**
- Create: `lib/McsCore/src/domain/WebFormSubmission.h`
- Create: `lib/McsCore/src/adapters/WebFormCommissioningAdapter.h`
- Modify: `test/support/FakeConfigStore.h` (add a `saved()` accessor — see Step 1)
- Modify: `test/test_fake_config_store/test_main.cpp` (cover the new accessor)
- Test: `test/test_web_form_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes: `CommissioningSession` (existing, `lib/McsCore/src/domain/CommissioningSession.h` — `CommissioningResult apply(const ParsedCommand&)`), `CommandLineParser` (existing, `lib/McsCore/src/domain/CommandLineParser.h` — `static ParsedCommand parse(const std::string&)`), `ParsedCommand` (existing — `static ParsedCommand save()`, `static ParsedCommand reboot()`), `FakeConfigStore` (existing, `test/support/FakeConfigStore.h` — currently has `save()`/`load()` only; this task adds `saved() const -> const std::optional<NodeConfig>&`).
- Produces: `WebFormSubmission` struct, `WebFormCommissioningAdapter` — consumed by Task 6's `CaptivePortalServer`.

**Note:** `FakeConfigStore` currently has no way to observe "was anything saved" independent of `load()`'s factory-default fallback. Add a `saved()` accessor as the first step below — it's a minimal, needed capability (mirrors why `FakePositionReporter` exposes `reports()`), not a speculative addition.

**Second deliberate deviation** (see Global Constraints) — no `#ifdef ARDUINO` guard, full native TDD, same rationale as Task 4 and as `SerialCommissioningAdapter`.

`WebFormSubmission` is a plain data holder for the raw text field values a captive-portal HTTP POST would contain (all as `std::string`, matching how HTML form fields arrive) — decouples this adapter from any Arduino `WebServer` type. Per-turnout fields are **optional**: an empty `pin` string for a turnout slot means "leave that turnout's existing configuration unchanged" (matches the design doc: "optional if the customer is wiring to your standard harness with defaults").

`WebFormCommissioningAdapter::submit()` reuses `CommandLineParser` by building the exact same command-line strings the serial workflow uses (`id ...`, `wifi "..." "..."`, `broker ...`, `turnout N pin ... fb ... orientation ... settle ... timeout ...`) and feeding each through `CommissioningSession::apply(CommandLineParser::parse(line))` — so the web path gets identical validation (bounds-checked turnout numbers, quoted SSIDs/passwords) for free, with zero duplicated parsing logic. Stops and returns the first `ERROR: ...` response without saving. On success, applies `save`, and only if that also succeeds, applies `reboot`.

- [ ] **Step 1: Add and test `FakeConfigStore::saved()`**

Add this test case to `test/test_fake_config_store/test_main.cpp`:

```cpp
TEST_CASE("FakeConfigStore::saved is empty until something is saved")
{
    FakeConfigStore store;
    REQUIRE_FALSE(store.saved().has_value());

    NodeConfig config = NodeConfig::factoryDefault().withId(NodeId(3));
    store.save(config);

    REQUIRE(store.saved().has_value());
    REQUIRE(*store.saved() == config);
}
```

Run: `pio test -e native -f test_fake_config_store` — expect FAIL to compile (`saved()` doesn't exist yet).

Add the accessor to `test/support/FakeConfigStore.h`:

```cpp
    const std::optional<NodeConfig>& saved() const
    {
        return saved_;
    }
```

(insert it as a public method, after `load()`). Run: `pio test -e native -f test_fake_config_store` — expect PASS, 4 test cases.

- [ ] **Step 2: Write the failing test**

```cpp
// test/test_web_form_commissioning_adapter/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/WebFormCommissioningAdapter.h"
#include "domain/CommissioningSession.h"
#include "domain/NodeConfig.h"
#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutConfig.h"
#include "domain/TurnoutId.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"
#include "support/FakeConfigStore.h"

// NOTE on turnout pin numbers below: NodeConfig::factoryDefault() gives every
// turnout the same sentinel pins (-1 output, -1 feedback), so a submission
// that only fills SOME turnouts while the rest sit at that sentinel would
// trip NodeConfig::validate()'s pin-conflict check (multiple turnouts
// "sharing" pin -1) and fail to save. Test submissions that expect a
// successful save therefore give every one of the 8 turnouts a distinct,
// non-conflicting pin pair.

namespace
{
WebFormSubmission fullyValidSubmission()
{
    WebFormSubmission form;
    form.nodeId = "3";
    form.wifiSsid = "Layout Room";
    form.wifiPassword = "blue caboose 42";
    form.brokerHost = "192.168.1.50";
    form.brokerPort = "1883";

    for (size_t i = 0; i < form.turnouts.size(); ++i)
    {
        int outputPin = 10 + static_cast<int>(i) * 2;
        int feedbackPin = 11 + static_cast<int>(i) * 2;
        form.turnouts[i].pin = std::to_string(outputPin);
        form.turnouts[i].feedbackPin = std::to_string(feedbackPin);
        form.turnouts[i].orientation = "normal";
        form.turnouts[i].settleMs = "300";
        form.turnouts[i].timeoutMs = "4000";
    }

    return form;
}
}

TEST_CASE("WebFormCommissioningAdapter applies a fully valid submission and reboots")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    std::string response = adapter.submit(fullyValidSubmission());

    REQUIRE(response == "REBOOTING");
    REQUIRE(store.saved().has_value());
    REQUIRE(store.saved()->id().value() == 3);
    REQUIRE(store.saved()->wifi().ssid() == "Layout Room");
    REQUIRE(store.saved()->wifi().password() == "blue caboose 42");
    REQUIRE(store.saved()->broker().host() == "192.168.1.50");
    REQUIRE(store.saved()->broker().port() == 1883);
    REQUIRE(store.saved()->turnouts()[0].outputPin() == 10);
    REQUIRE(store.saved()->turnouts()[7].outputPin() == 24);
}

TEST_CASE("WebFormCommissioningAdapter leaves turnout slots with an empty pin unchanged")
{
    // Seed the store with an already-valid config (distinct pins across all
    // 8 turnouts) so leaving some turnout fields blank doesn't collide with
    // the factory-default -1/-1 sentinel - see the file-level NOTE above.
    FakeConfigStore store;
    Orientation orientation = Orientation::normal();
    Duration settle(50);
    Duration timeout(200);
    std::array<TurnoutConfig, 8> seedTurnouts{
        TurnoutConfig(TurnoutId(1), 21, 31, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(2), 22, 32, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(3), 23, 33, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(4), 24, 34, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(5), 25, 35, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(6), 26, 36, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(7), 27, 37, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(8), 28, 38, orientation, settle, timeout)};
    NodeConfig seed(NodeId(5), WifiCredentials("existing", "pw"), BrokerAddress("10.0.0.1", 1883), seedTurnouts);
    store.save(seed);

    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form;
    form.nodeId = "5";
    form.wifiSsid = "existing";
    form.wifiPassword = "pw";
    form.brokerHost = "10.0.0.1";
    form.brokerPort = "1883";
    // Only fill in turnout 1, with a pin distinct from every seeded turnout;
    // turnouts 2-8 stay unset (optional) and should keep their seeded pins.
    form.turnouts[0].pin = "12";
    form.turnouts[0].feedbackPin = "13";
    form.turnouts[0].orientation = "normal";
    form.turnouts[0].settleMs = "300";
    form.turnouts[0].timeoutMs = "4000";

    std::string response = adapter.submit(form);

    REQUIRE(response == "REBOOTING");
    REQUIRE(store.saved().has_value());
    REQUIRE(store.saved()->turnouts()[0].outputPin() == 12);
    REQUIRE(store.saved()->turnouts()[1].outputPin() == 22);
    REQUIRE(store.saved()->turnouts()[7].outputPin() == 28);
}

TEST_CASE("WebFormCommissioningAdapter stops at the first error and does not save")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = fullyValidSubmission();
    form.brokerPort = "not-a-number";

    std::string response = adapter.submit(form);

    REQUIRE(response.rfind("ERROR", 0) == 0);
    REQUIRE_FALSE(store.saved().has_value());
}

TEST_CASE("WebFormCommissioningAdapter reports a validation error from save and does not reboot")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = fullyValidSubmission();
    form.nodeId = "0"; // invalid: fails NodeConfig::validate()

    std::string response = adapter.submit(form);

    REQUIRE(response.rfind("ERROR", 0) == 0);
    REQUIRE_FALSE(store.saved().has_value());
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: FAIL to compile — neither header exists yet.

- [ ] **Step 4: Write minimal implementation**

```cpp
// lib/McsCore/src/domain/WebFormSubmission.h
#pragma once

#include <array>
#include <string>

struct WebFormTurnoutField
{
    std::string pin;
    std::string feedbackPin;
    std::string orientation;
    std::string settleMs;
    std::string timeoutMs;
};

struct WebFormSubmission
{
    std::string nodeId;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string brokerHost;
    std::string brokerPort;
    std::array<WebFormTurnoutField, 8> turnouts;
};
```

```cpp
// lib/McsCore/src/adapters/WebFormCommissioningAdapter.h
#pragma once

#include <string>
#include <vector>

#include "domain/WebFormSubmission.h"
#include "domain/CommissioningSession.h"
#include "domain/CommandLineParser.h"
#include "domain/ParsedCommand.h"

class WebFormCommissioningAdapter
{
public:
    explicit WebFormCommissioningAdapter(CommissioningSession& session) : session_(session)
    {
    }

    std::string submit(const WebFormSubmission& form)
    {
        std::vector<std::string> lines = buildCommandLines(form);

        for (const std::string& line : lines)
        {
            CommissioningResult result = session_.apply(CommandLineParser::parse(line));
            if (isError(result.response))
            {
                return result.response;
            }
        }

        CommissioningResult saveResult = session_.apply(ParsedCommand::save());
        if (isError(saveResult.response))
        {
            return saveResult.response;
        }

        CommissioningResult rebootResult = session_.apply(ParsedCommand::reboot());
        return rebootResult.response;
    }

private:
    static bool isError(const std::string& response)
    {
        return response.rfind("ERROR", 0) == 0;
    }

    static std::vector<std::string> buildCommandLines(const WebFormSubmission& form)
    {
        std::vector<std::string> lines;
        lines.push_back("id " + form.nodeId);
        lines.push_back("wifi \"" + form.wifiSsid + "\" \"" + form.wifiPassword + "\"");
        lines.push_back("broker " + form.brokerHost + " " + form.brokerPort);

        for (size_t i = 0; i < form.turnouts.size(); ++i)
        {
            const WebFormTurnoutField& field = form.turnouts[i];
            if (field.pin.empty())
            {
                continue;
            }

            lines.push_back("turnout " + std::to_string(i + 1)
                + " pin " + field.pin
                + " fb " + field.feedbackPin
                + " orientation " + field.orientation
                + " settle " + field.settleMs
                + " timeout " + field.timeoutMs);
        }

        return lines;
    }

    CommissioningSession& session_;
};
```

- [ ] **Step 5: Run test to verify it passes**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: PASS, 4 test cases.

- [ ] **Step 6: Commit**

```bash
git add lib/McsCore/src/domain/WebFormSubmission.h lib/McsCore/src/adapters/WebFormCommissioningAdapter.h \
        test/support/FakeConfigStore.h test/test_fake_config_store/test_main.cpp \
        test/test_web_form_commissioning_adapter/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add WebFormCommissioningAdapter (native-tested, no ARDUINO guard)

Reuses CommandLineParser/CommissioningSession so the web-form path gets
the same validation as the serial path for free. Deliberate deviation
from the adapter/ARDUINO-guard convention - see ButtonSetupModeTrigger
and SerialCommissioningAdapter (backlog #18) for the same rationale.

EOF
)"
```

---

### Task 6: `EspDeviceIdentity` + `CaptivePortalServer` (build-check only, Arduino)

**Files:**
- Create: `lib/McsCore/src/adapters/EspDeviceIdentity.h`
- Create: `lib/McsCore/src/adapters/CaptivePortalServer.h`
- Modify (temporarily, then revert): `src/main.cpp`

**Interfaces:**
- Consumes: `DeviceIdentity` port (Task 3), `MacAddress` (Task 1), `SetupApName` (Task 2), `WebFormSubmission`/`WebFormCommissioningAdapter` (Task 5).
- Produces: nothing consumed by a later task in this plan — these are leaf adapters, verified only by build-check (no native test exists or is possible for real WiFi/DNS/HTTP hardware).

Both are `#ifdef ARDUINO`-guarded, matching every other hardware adapter in `lib/McsCore/src/adapters/` (`EspDigitalOutput`, `WiFiLink`, `NvsConfigStore`, etc.) — **not** the deviation from Tasks 4-5.

`EspDeviceIdentity` reads the ESP32's burned-in MAC via `WiFi.macAddress(uint8_t*)`.

`CaptivePortalServer` runs the setup-mode AP (`WiFi.softAP`), a DNS server that answers every query with the AP's own IP (captive-portal redirect, via the ESP32 Arduino core's `DNSServer` class), and an HTTP server (`WebServer`, also ESP32 Arduino core, no extra `lib_deps`) that serves a minimal HTML form and, on POST, extracts field values into a `WebFormSubmission` and calls `WebFormCommissioningAdapter::submit()`. `begin()` starts the AP/DNS/HTTP server; `poll()` — non-blocking, called from a loop — services DNS and HTTP requests (`dnsServer_.processNextRequest()`, `webServer_.handleClient()`).

- [ ] **Step 1: Write `EspDeviceIdentity`**

```cpp
// lib/McsCore/src/adapters/EspDeviceIdentity.h
#pragma once

#ifdef ARDUINO

#include <WiFi.h>

#include "ports/DeviceIdentity.h"
#include "domain/MacAddress.h"

class EspDeviceIdentity : public DeviceIdentity
{
public:
    MacAddress mac() const override
    {
        std::array<uint8_t, 6> bytes{};
        WiFi.macAddress(bytes.data());
        return MacAddress(bytes);
    }
};

#endif
```

- [ ] **Step 2: Write `CaptivePortalServer`**

```cpp
// lib/McsCore/src/adapters/CaptivePortalServer.h
#pragma once

#ifdef ARDUINO

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include <string>

#include "domain/MacAddress.h"
#include "domain/SetupApName.h"
#include "domain/WebFormSubmission.h"
#include "adapters/WebFormCommissioningAdapter.h"

class CaptivePortalServer
{
public:
    CaptivePortalServer(WebFormCommissioningAdapter& adapter, MacAddress apMac)
        : adapter_(adapter), apMac_(apMac), webServer_(80)
    {
    }

    void begin()
    {
        std::string apName = SetupApName::from(apMac_);
        WiFi.softAP(apName.c_str());

        IPAddress apIp = WiFi.softAPIP();
        dnsServer_.start(53, "*", apIp);

        webServer_.on("/", [this]() { handleRoot(); });
        webServer_.on("/submit", HTTP_POST, [this]() { handleSubmit(); });
        webServer_.onNotFound([this]() { handleRoot(); });
        webServer_.begin();
    }

    // Non-blocking - call repeatedly from a loop. No delay().
    void poll()
    {
        dnsServer_.processNextRequest();
        webServer_.handleClient();
    }

private:
    void handleRoot()
    {
        webServer_.send(200, "text/html", kFormHtml);
    }

    void handleSubmit()
    {
        WebFormSubmission form = readForm();
        std::string response = adapter_.submit(form);
        webServer_.send(200, "text/plain", response.c_str());
    }

    WebFormSubmission readForm()
    {
        WebFormSubmission form;
        form.nodeId = webServer_.arg("id").c_str();
        form.wifiSsid = webServer_.arg("wifi_ssid").c_str();
        form.wifiPassword = webServer_.arg("wifi_password").c_str();
        form.brokerHost = webServer_.arg("broker_host").c_str();
        form.brokerPort = webServer_.arg("broker_port").c_str();

        for (size_t i = 0; i < form.turnouts.size(); ++i)
        {
            std::string prefix = "t" + std::to_string(i + 1) + "_";
            form.turnouts[i].pin = webServer_.arg((prefix + "pin").c_str()).c_str();
            form.turnouts[i].feedbackPin = webServer_.arg((prefix + "fb").c_str()).c_str();
            form.turnouts[i].orientation = webServer_.arg((prefix + "orientation").c_str()).c_str();
            form.turnouts[i].settleMs = webServer_.arg((prefix + "settle").c_str()).c_str();
            form.turnouts[i].timeoutMs = webServer_.arg((prefix + "timeout").c_str()).c_str();
        }

        return form;
    }

    static constexpr const char* kFormHtml =
        "<html><body><h1>Tortoise Setup</h1>"
        "<form method='POST' action='/submit'>"
        "Node ID: <input name='id'><br>"
        "WiFi SSID: <input name='wifi_ssid'><br>"
        "WiFi Password: <input name='wifi_password' type='password'><br>"
        "Broker Host: <input name='broker_host'><br>"
        "Broker Port: <input name='broker_port'><br>"
        "<input type='submit' value='Save'>"
        "</form></body></html>";

    WebFormCommissioningAdapter& adapter_;
    MacAddress apMac_;
    DNSServer dnsServer_;
    WebServer webServer_;
};

#endif
```

- [ ] **Step 3: Build-check `EspDeviceIdentity`**

Temporarily add to `src/main.cpp` (record the file's original content first with `git diff src/main.cpp` — expect no output — so you can confirm an exact revert later):

```cpp
#include "adapters/EspDeviceIdentity.h"
```

and, inside `setup()`, a throwaway line that forces the compiler to instantiate it, e.g.:

```cpp
EspDeviceIdentity espDeviceIdentity;
(void)espDeviceIdentity.mac();
```

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 4: Build-check `CaptivePortalServer`**

Keeping the Task 3 temporary edit in place, additionally add:

```cpp
#include "adapters/CaptivePortalServer.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "domain/CommissioningSession.h"
#include "adapters/NvsConfigStore.h"
```

and, inside `setup()`:

```cpp
static NvsConfigStore configStoreForBuildCheck;
static CommissioningSession sessionForBuildCheck(configStoreForBuildCheck);
static WebFormCommissioningAdapter webAdapterForBuildCheck(sessionForBuildCheck);
static CaptivePortalServer captivePortalForBuildCheck(webAdapterForBuildCheck, espDeviceIdentity.mac());
captivePortalForBuildCheck.begin();
```

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 5: Revert `src/main.cpp`**

Revert every temporary edit from Steps 3-4 so `src/main.cpp` is byte-identical to what it was before this task. Verify:

```bash
git diff src/main.cpp
```

Expected: no output. Then rebuild once more to confirm the revert didn't break anything:

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
git add lib/McsCore/src/adapters/EspDeviceIdentity.h lib/McsCore/src/adapters/CaptivePortalServer.h
git commit -m "$(cat <<'EOF'
! F Add EspDeviceIdentity and CaptivePortalServer adapters

Build-check verified only (pio run -e esp32dev, src/main.cpp reverted
after) - no native equivalent exists for real WiFi/DNS/HTTP hardware.

EOF
)"
```

---

### Task 7: Update `docs/task-status.md`

**Files:**
- Modify: `docs/task-status.md`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing (docs only).

- [ ] **Step 1: Update the Completed table**

Add a row (after the bench serial commissioning row) documenting this feature, citing the real commit hashes from Tasks 1-6 above (look them up with `git log --oneline` — do not guess). Follow the existing row style exactly (see the `Bench serial commissioning` row for the level of detail expected).

- [ ] **Step 2: Update the Backlog table**

Remove backlog item `#19` from the Backlog table (it's now done). Leave `#20` as-is regardless of its current "Blocked by" status — a separate, concurrent plan may be updating item #20's status; do not assume its outcome. If a merge conflict arises later when this branch is merged, that is expected and should be resolved by combining both branches' additions, not by picking one side.

- [ ] **Step 3: Update the native test count**

Update the running native test binary count and the list of newly added binaries (this plan adds `test_mac_address`, `test_setup_ap_name`, `test_setup_mode_trigger_fakes`, `test_button_setup_mode_trigger`, `test_web_form_commissioning_adapter` — 5 new binaries; `EspDeviceIdentity`/`CaptivePortalServer` are build-check-only, not native binaries).

- [ ] **Step 4: Add a scaffolding-debt note**

In "Known scaffolding debt", add a bullet noting that `ButtonSetupModeTrigger`, `WebFormCommissioningAdapter`, `EspDeviceIdentity`, and `CaptivePortalServer` are not yet wired into `ControllerNode`/`src/main.cpp` — no boot-mode-selection logic exists yet to decide when setup mode should run instead of normal operation (same shape as the existing bench-serial-commissioning bullet).

- [ ] **Step 5: Commit**

```bash
git add docs/task-status.md
git commit -m "$(cat <<'EOF'
. d Mark Wireless commissioning complete in task-status.md

EOF
)"
```
