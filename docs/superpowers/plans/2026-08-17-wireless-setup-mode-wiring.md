# Wireless Setup Mode Wiring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the already-built, already-tested wireless commissioning classes (backlog #19: `ButtonSetupModeTrigger`, `EspDeviceIdentity`, `WebFormCommissioningAdapter`, `CaptivePortalServer`) into `src/main.cpp`, so holding **BOOT** through the boot window enters wireless captive-portal setup instead of normal operation — matching `docs/software-class-list.md`'s "Entering Setup Mode" workflow.

**Architecture:** Extends the `BootMode`/`BootModeSelector` pure domain class from backlog #22 with a third mode (`WirelessSetup`), which wins over both `Normal` and `NeedsCommissioning` whenever BOOT was held through the boot window — this lets a customer re-enter setup later even on an already-validly-configured board (e.g. to change WiFi networks), not just on a fresh factory-default one. `src/main.cpp` gets a small boot-time polling helper to determine BOOT-hold, then branches into exactly one of three states for the rest of the program's life: `ControllerNode` (normal), `CaptivePortalServer` (wireless setup), or neither (idle, needs-commissioning — unchanged from backlog #22). Bench serial commissioning (backlog #21) stays active in all three states, since it's an independent UART channel.

**Tech Stack:** PlatformIO, Catch2 3.7.1 (native test for the extended `BootModeSelector`), `esp32dev` build-check for the `main.cpp` change (no native equivalent for the composition root).

## Global Constraints

- Domain code must compile and run under `native` without `Arduino.h`. The extended `BootModeSelector` stays pure — no guard needed.
- No `delay()`, no dynamic allocation after boot.
- ACN notation for every commit message. Never `--amend`, never `--no-verify`.
- `src/main.cpp` has no native equivalent — verify with `pio run -e esp32dev` directly on the real file (no temporary-wire-then-revert; this *is* the permanent change).
- **Read `src/main.cpp` fresh before editing it** — it was changed by backlog #22 (commit `7cd9def`, merged to `main` in `b884b6e`). Confirm its current real content before working from this plan's description of it.
- **Accepted UX tradeoff, not an implementation gap:** detecting "was BOOT held through the whole boot window" fundamentally requires waiting out the full window before deciding — you cannot know a 2-second hold was sustained until 2 seconds have passed from a cold boot. This plan's boot-window check therefore adds a fixed ~2 second delay to **every** boot, whether or not BOOT is held. This mirrors how consumer devices with an equivalent "hold button to enter setup" pattern work (Tasmota, ESPHome, Shelly, etc.). Do not try to "optimize" this away by exiting early — `ButtonSetupModeTrigger`'s public interface deliberately doesn't expose partial early-release state for this purpose (see its own header/tests from backlog #19), and adding that would expand its interface just for this one call site.
- **GPIO 0 is the BOOT button** on ESP32-WROOM-32 dev boards — active-low (pressed = `Level::Low`), tied high via internal pull-up when released. Reading its state in `setup()` (well after the ROM bootloader's own strapping-pin decision has already resolved) is standard, safe practice, not a conflict with flashing/download mode.

---

### Task 1: Extend `BootMode`/`BootModeSelector` with `WirelessSetup`

**Files:**
- Modify: `lib/McsCore/src/domain/BootMode.h`
- Modify: `lib/McsCore/src/domain/BootModeSelector.h`
- Modify: `test/test_boot_mode_selector/test_main.cpp`

**Interfaces:**
- Consumes: `NodeConfig` (existing).
- Produces: `BootMode::WirelessSetup`, `BootModeSelector::select(const NodeConfig&, bool wirelessSetupRequested) -> BootMode` (signature change — the `bool` parameter is new) — consumed by Task 2's `src/main.cpp` wiring.

**Breaking change to an already-merged class, done deliberately via TDD, not left as a parallel/duplicate class:** `BootModeSelector::select` gains a second parameter. `wirelessSetupRequested == true` always wins, regardless of config validity — this lets a customer with an already-valid config hold BOOT to re-enter setup (e.g. to change WiFi networks), not just recover a factory-default board.

- [ ] **Step 1: Update the test file — full replacement**

Replace the full contents of `test/test_boot_mode_selector/test_main.cpp` with:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/BootModeSelector.h"
#include "domain/BootMode.h"
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
NodeConfig fullyValidConfig()
{
    Orientation orientation = Orientation::normal();
    Duration settle(50);
    Duration timeout(200);
    std::array<TurnoutConfig, 8> turnouts{
        TurnoutConfig(TurnoutId(1), 10, 11, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(2), 12, 13, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(3), 14, 15, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(4), 16, 17, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(5), 18, 19, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(6), 20, 21, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(7), 22, 23, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(8), 24, 25, orientation, settle, timeout)};
    return NodeConfig(NodeId(3), WifiCredentials("ssid", "pw"), BrokerAddress("host", 1883), turnouts);
}
}

TEST_CASE("BootModeSelector selects Normal for a fully valid config, no wireless setup request")
{
    REQUIRE(BootModeSelector::select(fullyValidConfig(), false) == BootMode::Normal);
}

TEST_CASE("BootModeSelector selects NeedsCommissioning for the factory default, no wireless setup request")
{
    REQUIRE(BootModeSelector::select(NodeConfig::factoryDefault(), false) == BootMode::NeedsCommissioning);
}

TEST_CASE("BootModeSelector selects NeedsCommissioning for an out-of-range node id alone")
{
    NodeConfig config = fullyValidConfig().withId(NodeId(0));
    REQUIRE(BootModeSelector::select(config, false) == BootMode::NeedsCommissioning);
}

TEST_CASE("BootModeSelector selects NeedsCommissioning for a pin conflict alone")
{
    Orientation orientation = Orientation::normal();
    Duration settle(50);
    Duration timeout(200);
    NodeConfig config = fullyValidConfig().withTurnout(1, TurnoutConfig(TurnoutId(2), 10, 11, orientation, settle, timeout));
    REQUIRE(BootModeSelector::select(config, false) == BootMode::NeedsCommissioning);
}

TEST_CASE("BootModeSelector selects WirelessSetup when requested, even with a fully valid config")
{
    REQUIRE(BootModeSelector::select(fullyValidConfig(), true) == BootMode::WirelessSetup);
}

TEST_CASE("BootModeSelector selects WirelessSetup when requested, even with an invalid config")
{
    REQUIRE(BootModeSelector::select(NodeConfig::factoryDefault(), true) == BootMode::WirelessSetup);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_boot_mode_selector`
Expected: FAIL to compile (`select` doesn't take 2 arguments yet, `WirelessSetup` doesn't exist).

- [ ] **Step 3: Update the implementation**

Replace the full contents of `lib/McsCore/src/domain/BootMode.h` with:

```cpp
#pragma once

enum class BootMode
{
    Normal,
    NeedsCommissioning,
    WirelessSetup
};
```

Replace the full contents of `lib/McsCore/src/domain/BootModeSelector.h` with:

```cpp
#pragma once

#include "domain/BootMode.h"
#include "domain/NodeConfig.h"

class BootModeSelector
{
public:
    static BootMode select(const NodeConfig& config, bool wirelessSetupRequested)
    {
        if (wirelessSetupRequested)
        {
            return BootMode::WirelessSetup;
        }

        return config.validate().empty() ? BootMode::Normal : BootMode::NeedsCommissioning;
    }
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_boot_mode_selector`
Expected: PASS, 6 test cases.

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: all 41 binaries still pass — `BootModeSelector` has no other callers yet at the source level (Task 2 adds the first one, in `main.cpp`, which isn't part of the native build).

- [ ] **Step 6: Commit**

```bash
git add lib/McsCore/src/domain/BootMode.h lib/McsCore/src/domain/BootModeSelector.h \
        test/test_boot_mode_selector/test_main.cpp
git commit -m "$(cat <<'EOF'
! F Add WirelessSetup mode to BootModeSelector

Extends the backlog #22 boot-mode decision with a third case: BOOT
held through the boot window wins over both Normal and
NeedsCommissioning, regardless of config validity - lets a customer
re-enter wireless setup on an already-configured board, not just
recover a factory-default one. Breaking signature change to an
already-merged class (select() gains a bool parameter), done via TDD
with the existing test file's 4 cases updated in place plus 2 new
ones - not a parallel/duplicate class.

EOF
)"
```

---

### Task 2: Wire `ButtonSetupModeTrigger`/`CaptivePortalServer` into `src/main.cpp`

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `BootModeSelector`/`BootMode` (Task 1), `ButtonSetupModeTrigger` (existing, `lib/McsCore/src/adapters/ButtonSetupModeTrigger.h` — `ButtonSetupModeTrigger(DigitalInput&, Duration)`, `void poll(Instant)`, `bool requested() const`), `EspDigitalInput` (existing, `lib/McsCore/src/adapters/EspDigitalInput.h` — `EspDigitalInput(int pin, bool hasInternalPullUp)`), `ArduinoClock` (existing, `lib/McsCore/src/adapters/ArduinoClock.h` — default-constructible, `Instant now() const`), `EspDeviceIdentity` (existing — default-constructible, `MacAddress mac() const`), `WebFormCommissioningAdapter` (existing, `lib/McsCore/src/adapters/WebFormCommissioningAdapter.h` — `explicit WebFormCommissioningAdapter(CommissioningSession&)`, `bool rebootRequested() const`), `CaptivePortalServer` (existing, `lib/McsCore/src/adapters/CaptivePortalServer.h` — `CaptivePortalServer(WebFormCommissioningAdapter&, MacAddress)`, `void begin()`, `void poll()`), everything backlog #21/#22 already wired.
- Produces: nothing consumed by a later task — composition root, end of the chain.

- [ ] **Step 1: Read the current file**

Read `src/main.cpp` in full first. Confirm it currently has the backlog #22 shape: `setup()` unconditionally constructs the bench-serial-commissioning objects, then loads `NodeConfig` and calls `BootModeSelector::select(config)` with **one** argument (Task 1 just changed that signature to two — this task's edit must pass the new second argument), branching only between constructing `ControllerNode` or leaving `node` as `nullptr`. If it doesn't match this description, stop and report rather than guessing.

- [ ] **Step 2: Make the change**

Replace the full contents of `src/main.cpp` with:

```cpp
#include <Arduino.h>

#include "adapters/ControllerNode.h"
#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ButtonSetupModeTrigger.h"
#include "adapters/EspDigitalInput.h"
#include "adapters/ArduinoClock.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "adapters/CaptivePortalServer.h"
#include "domain/CommissioningSession.h"
#include "domain/NodeConfig.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/Duration.h"
#include "domain/Instant.h"

namespace
{
// How long BOOT must be held through power-on to enter wireless setup mode.
// This is an unavoidable fixed delay on every boot, not just when BOOT is
// held - see this plan's Global Constraints for why.
constexpr unsigned long kBootWindowMs = 2000;

// GPIO 0 is the BOOT button on ESP32-WROOM-32 dev boards - active-low, tied
// high via internal pull-up when not pressed. Reading it here in setup() is
// well after the ROM bootloader's own strapping-pin decision has resolved.
bool detectWirelessSetupRequest()
{
    static ArduinoClock bootClock;
    static EspDigitalInput bootPin(0, true);
    static ButtonSetupModeTrigger trigger(bootPin, Duration(kBootWindowMs));

    Instant start = bootClock.now();
    trigger.poll(start);
    while ((bootClock.now() - start) < Duration(kBootWindowMs))
    {
        trigger.poll(bootClock.now());
    }

    return trigger.requested();
}
}

// ControllerNode is constructed here (function-local static, not file-scope)
// because its constructor performs real NVS/GPIO work via NvsConfigStore and
// the Esp*Output/Input adapters. A file-scope static would run before
// app_main() calls initArduino() (see cores/esp32/main.cpp), which is the
// only place nvs_flash_init() runs (esp32-hal-misc.c) - constructing before
// that silently drops all saved NVS config to factory defaults. setup() only
// runs after initArduino(), so this ordering is safe.
static ControllerNode* node = nullptr;
static SerialCommissioningAdapter* commissioningAdapter = nullptr;
static CaptivePortalServer* captivePortal = nullptr;
static WebFormCommissioningAdapter* webFormAdapter = nullptr;

void setup()
{
    // Bench serial commissioning runs in parallel with every other mode
    // below, regardless of config validity or wireless setup - it's a
    // distinct physical channel (UART) from both the turnout GPIO/MQTT
    // graph and the wireless setup AP/HTTP server.
    static NvsConfigStore commissioningStore;
    static CommissioningSession commissioningSession(commissioningStore);
    static EspUartPort uart(115200);
    static SerialCommissioningAdapter adapter(uart, commissioningSession);
    commissioningAdapter = &adapter;

    bool wirelessSetupRequested = detectWirelessSetupRequest();
    NodeConfig config = commissioningStore.load();
    BootMode mode = BootModeSelector::select(config, wirelessSetupRequested);

    if (mode == BootMode::WirelessSetup)
    {
        // Shares the same commissioningSession as bench serial - either
        // channel edits the same draft NodeConfig, both save() to the same
        // NvsConfigStore. ControllerNode is not constructed in this mode:
        // the AP takes over WiFi instead of connecting to the home network.
        static EspDeviceIdentity deviceIdentity;
        static WebFormCommissioningAdapter formAdapter(commissioningSession);
        webFormAdapter = &formAdapter;
        static CaptivePortalServer portal(formAdapter, deviceIdentity.mac());
        captivePortal = &portal;
        captivePortal->begin();
    }
    else if (mode == BootMode::Normal)
    {
        static ControllerNode instance;
        node = &instance;
        node->begin();
    }

    // BootMode::NeedsCommissioning: neither node nor captivePortal is
    // constructed - loop() below only runs the always-on serial channel.
}

void loop()
{
    if (node != nullptr)
    {
        node->tick();
    }

    if (captivePortal != nullptr)
    {
        captivePortal->poll();
        if (webFormAdapter->rebootRequested())
        {
            ESP.restart();
        }
    }

    commissioningAdapter->poll();
    if (commissioningAdapter->rebootRequested())
    {
        ESP.restart();
    }
}
```

- [ ] **Step 3: Build-check**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`. This is the real, permanent target file — there is no revert step.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "$(cat <<'EOF'
! F Wire wireless setup mode into main.cpp

Holding BOOT for 2s through power-on now enters wireless captive-
portal setup (CaptivePortalServer/WebFormCommissioningAdapter)
instead of normal operation, via the extended BootModeSelector.
Shares the same CommissioningSession as bench serial commissioning,
so either channel edits the same draft config. Build-check verified
only (pio run -e esp32dev) - main.cpp has no native equivalent.

EOF
)"
```

---

### Task 3: Update `docs/task-status.md`

**Files:**
- Modify: `docs/task-status.md`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing (docs only).

- [ ] **Step 1: Update the Completed table**

Add a row documenting this wiring, citing the real commit hashes from Tasks 1-2 (look them up with `git log --oneline` — do not guess).

- [ ] **Step 2: Remove the now-resolved "not yet wired" bullet for task #19's classes**

In "Known scaffolding debt", remove the bullet reading "`ButtonSetupModeTrigger`, `WebFormCommissioningAdapter`, `EspDeviceIdentity`, and `CaptivePortalServer` (task #19) are not yet wired into `ControllerNode`/`src/main.cpp`..." — it's resolved by this plan.

- [ ] **Step 3: Leave the captive-portal turnout-fields gap bullet as-is**

The bullet describing `CaptivePortalServer`'s served form lacking turnout fields (the factory-default pin-conflict gap) is **not** resolved by this plan — it's backlog #24, a separate, not-yet-scheduled product decision. Do not remove or edit that bullet; this plan only wires the existing (still-gapped) form into `main.cpp`, it doesn't fix the gap itself.

- [ ] **Step 4: Commit**

```bash
git add docs/task-status.md
git commit -m "$(cat <<'EOF'
. d Mark wireless setup mode wiring complete in task-status.md

EOF
)"
```
