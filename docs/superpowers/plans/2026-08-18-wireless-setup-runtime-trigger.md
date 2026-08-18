# Wireless Setup Runtime Trigger Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the "hold BOOT through power-on" wireless-setup gesture, which is physically impossible on ESP32-WROOM-32 hardware — it collides with the chip's own GPIO0 boot-strapping pin and puts the board into permanent UART download mode instead of running the app at all (confirmed on real hardware: `rst:0x1 (POWERON_RESET),boot:0x3 (DOWNLOAD_BOOT...)`, `waiting for download`). Replace it with a long-hold-then-release gesture detected live during normal runtime (`loop()`), the same way `ButtonIdentifyRequestTrigger`'s short-press already works safely today.

**Architecture:** `ButtonSetupModeTrigger` changes from "was BOOT held continuously through a fixed boot-time window" (checked once, blocking, in `setup()`) to "was BOOT held for at least a minimum duration, then released" (edge-triggered, polled every `loop()` iteration alongside `ButtonIdentifyRequestTrigger`, same physical pin). Because the trigger now fires *after* boot instead of *at* boot, `BootModeSelector::select`'s `wirelessSetupRequested` boolean can no longer come from a live pin read at boot time — it now comes from a small persisted flag (`SetupModeRequestStore`/`NvsSetupModeRequestStore`, new port+adapter, separate NVS namespace from `NodeConfig`) that the runtime trigger sets just before calling `ESP.restart()`, and that `setup()` reads-and-clears on the next boot. This removes the previous design's "unavoidable fixed ~2s delay on every boot" as a side effect, since boot-time button polling goes away entirely.

**Tech Stack:** PlatformIO, Catch2 3.7.1 (native tests for `SetupModeRequestStore`'s fake and the redefined `ButtonSetupModeTrigger`), `esp32dev` build-check + on-device verification for the `main.cpp` change (no native equivalent).

## Global Constraints

- Domain/port code must compile and run under `native` without `Arduino.h`. `SetupModeRequestStore` (port) and `FakeSetupModeRequestStore` stay pure — no guard needed. `NvsSetupModeRequestStore` is `#ifdef ARDUINO`-guarded like `NvsConfigStore`, with no native test (matches `NvsConfigStore`'s existing precedent — verify by build-check only).
- No `delay()`, no dynamic allocation after boot.
- **Commit via the `/arlo-commits` skill, per this repo's `CLAUDE.md` — do not hand-write commit messages or run `git commit` directly**, even though earlier plans in `docs/superpowers/plans/` show inline `git commit -m` steps with hand-written ACN notation. Each task below ends with "commit via `/arlo-commits`" rather than a literal command.
- `src/main.cpp` has no native equivalent — verify with `pio run -e esp32dev` directly on the real file, then verify on real hardware (flash + serial monitor + physically operate BOOT) — this bug was only caught by on-device testing, not the native suite.
- **Read `src/main.cpp` fresh before editing it** (Task 4) — confirm its current real content matches this plan's description before working from it.
- **The core hardware fact driving this whole plan:** GPIO0 (BOOT) is an ESP32 strapping pin. The ROM bootloader samples it once, at reset release, before any application code exists in memory — if low, the ROM enters permanent UART download mode and never runs the flashed app. There is no way to read GPIO0 in `setup()`/`loop()` in a way that reconstructs a hold that started *before* reset — any app-level detection of a BOOT gesture must be a hold that both starts and ends *after* boot has already completed normally.

---

### Task 1: Add `SetupModeRequestStore` port + `FakeSetupModeRequestStore`

**Files:**
- Create: `lib/McsCore/src/ports/SetupModeRequestStore.h`
- Create: `test/support/FakeSetupModeRequestStore.h`
- Modify: `test/test_setup_mode_trigger_fakes/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `SetupModeRequestStore` (port) — `void requestOnNextBoot()`, `bool consumeRequest()` (returns whether a request was pending, and clears it) — consumed abstractly by nothing yet in this plan (Task 2's adapter implements it; Task 4's `main.cpp` uses the adapter concretely, following the same pattern as `NvsConfigStore`/`ConfigStore`). `FakeSetupModeRequestStore` — same interface, in-memory, for native tests.

- [ ] **Step 1: Write the port**

Create `lib/McsCore/src/ports/SetupModeRequestStore.h`:

```cpp
#pragma once

class SetupModeRequestStore
{
public:
    virtual ~SetupModeRequestStore() = default;
    virtual void requestOnNextBoot() = 0;
    virtual bool consumeRequest() = 0;
};
```

- [ ] **Step 2: Write the fake**

Create `test/support/FakeSetupModeRequestStore.h`:

```cpp
#pragma once

#include "ports/SetupModeRequestStore.h"

class FakeSetupModeRequestStore : public SetupModeRequestStore
{
public:
    void requestOnNextBoot() override
    {
        pending_ = true;
    }

    bool consumeRequest() override
    {
        bool wasPending = pending_;
        pending_ = false;
        return wasPending;
    }

private:
    bool pending_ = false;
};
```

- [ ] **Step 3: Write the failing test**

Append to `test/test_setup_mode_trigger_fakes/test_main.cpp` (after the existing `TEST_CASE`s, before nothing else needed — just add these two, plus the include):

```cpp
#include "support/FakeSetupModeRequestStore.h"

TEST_CASE("FakeSetupModeRequestStore has no pending request by default")
{
    FakeSetupModeRequestStore store;
    REQUIRE_FALSE(store.consumeRequest());
}

TEST_CASE("FakeSetupModeRequestStore reports and clears a pending request")
{
    FakeSetupModeRequestStore store;
    store.requestOnNextBoot();

    REQUIRE(store.consumeRequest());
    REQUIRE_FALSE(store.consumeRequest());
}
```

Add the `#include "support/FakeSetupModeRequestStore.h"` line near the top with the other includes, not inline mid-file.

- [ ] **Step 4: Run test to verify it fails**

Run: `pio test -e native -f test_setup_mode_trigger_fakes`
Expected: FAIL to compile (`FakeSetupModeRequestStore.h` doesn't exist yet) — if Steps 1-2 are done first this will instead pass immediately; run this step only to confirm the new `TEST_CASE`s execute, not as a strict red-first gate (the class under test is trivial and was written in Step 2).

- [ ] **Step 5: Run test to verify it passes**

Run: `pio test -e native -f test_setup_mode_trigger_fakes`
Expected: PASS, 4 test cases (2 existing + 2 new).

- [ ] **Step 6: Run the full native suite**

Run: `pio test -e native`
Expected: all binaries pass, no regressions.

- [ ] **Step 7: Commit**

Commit via the `/arlo-commits` skill.

---

### Task 2: Add `NvsSetupModeRequestStore` adapter

**Files:**
- Create: `lib/McsCore/src/adapters/NvsSetupModeRequestStore.h`

**Interfaces:**
- Consumes: `SetupModeRequestStore` (Task 1).
- Produces: `NvsSetupModeRequestStore` — concrete class, default-constructible, implements `SetupModeRequestStore` — consumed concretely by Task 4's `main.cpp` (same pattern as `NvsConfigStore`: constructed as a function-local `static` in `setup()`, held via a pointer for `loop()` to use).

- [ ] **Step 1: Write the adapter**

Create `lib/McsCore/src/adapters/NvsSetupModeRequestStore.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <Preferences.h>

#include "ports/SetupModeRequestStore.h"

// Separate NVS namespace from NvsConfigStore's "mcs-cfg" - this is a
// transient boot-intent flag, not part of the node's actual configuration.
class NvsSetupModeRequestStore : public SetupModeRequestStore
{
public:
    void requestOnNextBoot() override
    {
        Preferences prefs;
        prefs.begin(kNamespace, false);
        prefs.putBool(kKey, true);
        prefs.end();
    }

    bool consumeRequest() override
    {
        Preferences prefs;
        prefs.begin(kNamespace, false);
        bool pending = prefs.getBool(kKey, false);
        if (pending)
        {
            prefs.putBool(kKey, false);
        }
        prefs.end();
        return pending;
    }

private:
    static constexpr const char* kNamespace = "mcs-boot";
    static constexpr const char* kKey = "wsetup";
};

#endif
```

- [ ] **Step 2: Build-check**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`. `NvsSetupModeRequestStore` isn't referenced by any other file yet, so this only proves the new header compiles standalone via the LDF pulling it in — if PlatformIO doesn't compile it in isolation, that's fine; Task 4 wires it in and re-verifies.

- [ ] **Step 3: Commit**

Commit via the `/arlo-commits` skill.

---

### Task 3: Redefine `ButtonSetupModeTrigger` for runtime hold-and-release detection

**Files:**
- Modify: `lib/McsCore/src/adapters/ButtonSetupModeTrigger.h`
- Modify: `test/test_button_setup_mode_trigger/test_main.cpp`

**Interfaces:**
- Consumes: `DigitalInput` (existing), `Duration`/`Instant` (existing).
- Produces: `ButtonSetupModeTrigger(DigitalInput& bootPin, Duration minHoldDuration)` — **breaking signature/semantics change** to an already-merged class (constructor's second parameter changes meaning from "boot window length" to "minimum hold duration before a release counts"; `poll(Instant)` is no longer a boot-time blocking-loop helper's callee, it's a per-`loop()`-tick call), `void poll(Instant now)`, `bool requested() const` (edge-triggered — true only on the tick BOOT is released after a qualifying hold, mirrors `ButtonIdentifyRequestTrigger`'s edge-triggering). Consumed by Task 4's `main.cpp`.

**Breaking change, done deliberately via TDD, not left as a parallel/duplicate class** — same class name and port (`SetupModeTrigger`), same *responsibility* ("detect that BOOT was used to request wireless setup"), different physical detection mechanism, because the old mechanism cannot work on this hardware (see Global Constraints).

- [ ] **Step 1: Read the current file**

Read `lib/McsCore/src/adapters/ButtonSetupModeTrigger.h` in full first. Confirm it currently matches the "held continuously from the first poll through the full window" implementation described in this plan's Architecture section. If it doesn't, stop and report rather than guessing.

- [ ] **Step 2: Replace the test file**

Replace the full contents of `test/test_button_setup_mode_trigger/test_main.cpp` with:

```cpp
// test/test_button_setup_mode_trigger/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/ButtonSetupModeTrigger.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("ButtonSetupModeTrigger is not requested while the button is still held")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    ButtonSetupModeTrigger trigger(bootPin, Duration(3000));

    trigger.poll(Instant(0));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger fires on the tick a qualifying long hold is released")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonSetupModeTrigger trigger(bootPin, Duration(3000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(3200));

    REQUIRE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger is edge-triggered - true for one tick only")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    bootPin.enqueue(Level::High);
    ButtonSetupModeTrigger trigger(bootPin, Duration(3000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(3200));
    REQUIRE(trigger.requested());

    trigger.poll(Instant(3400));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger ignores a release before the minimum hold duration")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonSetupModeTrigger trigger(bootPin, Duration(3000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(1500));

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonSetupModeTrigger can fire again on a second qualifying hold and release")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonSetupModeTrigger trigger(bootPin, Duration(3000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(3200));
    REQUIRE(trigger.requested());

    trigger.poll(Instant(4000));
    trigger.poll(Instant(7300));
    REQUIRE(trigger.requested());
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `pio test -e native -f test_button_setup_mode_trigger`
Expected: FAIL — the old implementation requires the pin to be `Low` on every single poll starting from the first one, so "fires on release after a hold" behavior doesn't exist yet (most new cases will fail or the file won't express the old API correctly).

- [ ] **Step 4: Replace the implementation**

Replace the full contents of `lib/McsCore/src/adapters/ButtonSetupModeTrigger.h` with:

```cpp
#pragma once

#include "ports/SetupModeTrigger.h"
#include "ports/DigitalInput.h"
#include "domain/Level.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

// Detects "hold BOOT for at least minHoldDuration, then release it" during
// normal runtime polling - deliberately NOT a boot-time strapping-pin read.
// GPIO0 (BOOT) is the ESP32's own boot-mode strapping pin: holding it low
// through a power-on or reset puts the ROM bootloader into permanent UART
// download mode before any application code runs, so a "held through
// power-on" gesture can never be observed from setup()/loop() at all. This
// must instead be read live, well after boot has already completed
// normally - exactly like this same pin's ButtonIdentifyRequestTrigger
// short-press already does.
class ButtonSetupModeTrigger : public SetupModeTrigger
{
public:
    ButtonSetupModeTrigger(DigitalInput& bootPin, Duration minHoldDuration)
        : bootPin_(bootPin), minHoldDuration_(minHoldDuration)
    {
    }

    // Call repeatedly with the current time during normal operation.
    // Non-blocking - no delay(). Edge-triggered: fires the tick BOOT is
    // released, but only if it had been held continuously for at least
    // minHoldDuration_ first.
    void poll(Instant now)
    {
        Level level = bootPin_.read();
        requestedThisTick_ = false;

        if (level == Level::Low && !pressed_)
        {
            pressed_ = true;
            pressStart_ = now;
        }
        else if (level == Level::High && pressed_)
        {
            pressed_ = false;
            Duration heldFor = now - pressStart_;
            if (heldFor >= minHoldDuration_)
            {
                requestedThisTick_ = true;
            }
        }
    }

    bool requested() const override
    {
        return requestedThisTick_;
    }

private:
    DigitalInput& bootPin_;
    Duration minHoldDuration_;
    bool pressed_ = false;
    Instant pressStart_ = Instant(0);
    bool requestedThisTick_ = false;
};
```

- [ ] **Step 5: Run test to verify it passes**

Run: `pio test -e native -f test_button_setup_mode_trigger`
Expected: PASS, 5 test cases.

- [ ] **Step 6: Run the full native suite**

Run: `pio test -e native`
Expected: all binaries pass. `ButtonSetupModeTrigger` has no other callers at the source level yet (Task 4 adds the only one, in `main.cpp`, which isn't part of the native build).

- [ ] **Step 7: Commit**

Commit via the `/arlo-commits` skill.

---

### Task 4: Wire the runtime trigger into `src/main.cpp`

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `NvsSetupModeRequestStore` (Task 2), `ButtonSetupModeTrigger` (Task 3, new constructor signature), everything else already wired (`ControllerNode`, `ButtonIdentifyRequestTrigger`, `EspDigitalInput`, `EspDigitalOutput`, `ArduinoClock`, `CaptivePortalServer`, `WebFormCommissioningAdapter`, `BlinkOutIdentifier`, `SteadyBlinker`, `Deadline`, `BootModeSelector`).
- Produces: nothing consumed by a later task — composition root, end of the chain.

- [ ] **Step 1: Read the current file**

Read `src/main.cpp` in full first (per Global Constraints). Confirm it currently has the shape described in this plan's Architecture section: `detectWirelessSetupRequest(EspDigitalInput&)` busy-waits `kBootWindowMs` in `setup()` using the *old* `ButtonSetupModeTrigger` API. If it doesn't match, stop and report rather than guessing.

- [ ] **Step 2: Make the change**

Replace the full contents of `src/main.cpp` with:

```cpp
#include <Arduino.h>

#include "adapters/ControllerNode.h"
#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ButtonSetupModeTrigger.h"
#include "adapters/ButtonIdentifyRequestTrigger.h"
#include "adapters/EspDigitalInput.h"
#include "adapters/EspDigitalOutput.h"
#include "adapters/ArduinoClock.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "adapters/CaptivePortalServer.h"
#include "domain/CommissioningSession.h"
#include "domain/NodeConfig.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/BlinkOutIdentifier.h"
#include "domain/SteadyBlinker.h"
#include "domain/Deadline.h"
#include "domain/Level.h"
#include "domain/Duration.h"
#include "domain/Instant.h"

namespace
{
// Short-press window for the runtime identify-blink trigger (field
// identification) - reuses the same physical BOOT pin, per
// docs/software-class-list.md's "Field Identification: Blink-Out" design.
constexpr unsigned long kIdentifyMinPressMs = 50;
constexpr unsigned long kIdentifyMaxPressMs = 1500;

// How long BOOT must be held, during normal runtime, before releasing it
// re-enters wireless setup on the next boot. Deliberately well above
// kIdentifyMaxPressMs above so a single release can never satisfy both
// triggers. Read live in loop() rather than at boot time - see
// ButtonSetupModeTrigger.h for why holding BOOT through an ESP32 power-on
// can't be detected in application code at all.
constexpr unsigned long kSetupModeHoldMs = 3000;

// How long the identify-blink sequence stays active after a qualifying
// short press, before the status LED goes dark again.
constexpr unsigned long kIdentifyActiveMs = 5000;

// Blink half-period for the distinct collision-error pattern - steady fast
// blink, visually different from the per-id identify pattern.
constexpr unsigned long kCollisionBlinkHalfPeriodMs = 250;

// Blink half-period for the setup-mode indicator - rapid steady blink,
// faster than the collision pattern above so the two are distinguishable
// even though they never occur at the same time (mutually exclusive boot
// modes). Lets a customer match the AP name in their WiFi list to the
// physical board - see docs/software-class-list.md's "Entering Setup Mode".
constexpr unsigned long kSetupModeBlinkHalfPeriodMs = 100;
}

// ControllerNode is constructed here (function-local static, not file-scope)
// because its constructor performs real NVS/GPIO work via NvsConfigStore and
// the Esp*Output/Input adapters. A file-scope static would run before
// app_main() calls initArduino() (see cores/esp32/main.cpp), which is the
// only place nvs_flash_init() runs (esp32-hal-misc.c) - constructing before
// that silently drops all saved NVS config to factory defaults. setup() only
// runs after initArduino(), so this ordering is safe. Every other
// hardware-touching adapter below follows the same rule; plain domain value
// objects (Deadline, Instant, bool) don't touch hardware and are safe as
// ordinary file-scope statics.
static ControllerNode* node = nullptr;
static SerialCommissioningAdapter* commissioningAdapter = nullptr;
static CaptivePortalServer* captivePortal = nullptr;
static WebFormCommissioningAdapter* webFormAdapter = nullptr;
static EspDigitalOutput* statusLed = nullptr;
static ArduinoClock* blinkClock = nullptr;
static ButtonIdentifyRequestTrigger* identifyTrigger = nullptr;
static BlinkOutIdentifier* blinkIdentifier = nullptr;
static SteadyBlinker* collisionBlinker = nullptr;
static SteadyBlinker* setupModeBlinker = nullptr;
static ButtonSetupModeTrigger* runtimeSetupTrigger = nullptr;
static NvsSetupModeRequestStore* setupRequestStore = nullptr;
static Deadline identifyDeadline;
static Instant identifyStart(0);

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

    static NvsSetupModeRequestStore requestStore;
    setupRequestStore = &requestStore;
    bool wirelessSetupRequested = requestStore.consumeRequest();

    NodeConfig config = commissioningStore.load();
    BootMode mode = BootModeSelector::select(config, wirelessSetupRequested);

    // Shared with the runtime identify/setup-mode triggers below - same
    // physical BOOT pin, distinguished by hold duration.
    static EspDigitalInput bootPin(0, true);

    // Status LED (GPIO 2) and its clock are shared across all three boot
    // modes - only the pattern driving them differs.
    static EspDigitalOutput led(2);
    statusLed = &led;
    static ArduinoClock ledClock;
    blinkClock = &ledClock;

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

        // Rapid steady blink signals "this board is in setup mode" - see
        // kSetupModeBlinkHalfPeriodMs above.
        static SteadyBlinker setupBlinker{Duration(kSetupModeBlinkHalfPeriodMs)};
        setupModeBlinker = &setupBlinker;
    }
    else
    {
        // Normal and NeedsCommissioning both watch BOOT for the runtime
        // gesture that (re-)enters wireless setup: hold for
        // kSetupModeHoldMs, then release. This is the only way a
        // factory-fresh board (NeedsCommissioning, no valid config yet)
        // reaches wireless setup at all.
        static ButtonSetupModeTrigger setupTrigger(bootPin, Duration(kSetupModeHoldMs));
        runtimeSetupTrigger = &setupTrigger;

        if (mode == BootMode::Normal)
        {
            static ControllerNode instance;
            node = &instance;
            node->begin();

            // Field identification (short-press BOOT blinks the node's id)
            // and the distinct collision-error pattern (steady fast blink,
            // driven instead whenever ControllerNode::blocked() is true)
            // share the status LED - only meaningful once a node has an
            // actual id, so these are only constructed in Normal mode.
            static ButtonIdentifyRequestTrigger trigger(bootPin, Duration(kIdentifyMinPressMs), Duration(kIdentifyMaxPressMs));
            identifyTrigger = &trigger;
            static BlinkOutIdentifier identifier(config.id(), Duration(200), Duration(200), Duration(1000));
            blinkIdentifier = &identifier;
            static SteadyBlinker errorBlinker{Duration(kCollisionBlinkHalfPeriodMs)};
            collisionBlinker = &errorBlinker;
        }
    }
}

void loop()
{
    Instant now = blinkClock->now();

    if (runtimeSetupTrigger != nullptr)
    {
        runtimeSetupTrigger->poll(now);
        if (runtimeSetupTrigger->requested())
        {
            // BOOT was already observed released (see ButtonSetupModeTrigger)
            // before this fires, so it's safe to restart here - GPIO0 won't
            // be held low at the ROM's next strapping-pin sample.
            setupRequestStore->requestOnNextBoot();
            ESP.restart();
        }
    }

    if (node != nullptr)
    {
        node->tick();

        if (node->blocked())
        {
            statusLed->write(collisionBlinker->levelAt(now - Instant(0)));
        }
        else
        {
            identifyTrigger->poll(now);
            if (identifyTrigger->requested())
            {
                identifyDeadline.arm(now, Duration(kIdentifyActiveMs));
                identifyStart = now;
            }

            bool identifying = identifyDeadline.armed() && !identifyDeadline.expired(now);
            statusLed->write(identifying ? blinkIdentifier->levelAt(now - identifyStart) : Level::Low);
        }
    }

    if (captivePortal != nullptr)
    {
        captivePortal->poll();
        statusLed->write(setupModeBlinker->levelAt(now - Instant(0)));

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
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

Commit via the `/arlo-commits` skill.

---

### Task 5: Update docs

**Files:**
- Modify: `docs/first-time-setup.md`
- Modify: `docs/software-class-list.md`
- Modify: `docs/task-status.md`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing (docs only).

- [ ] **Step 1: Rewrite `docs/first-time-setup.md` Step 1**

Replace:

```markdown
## Step 1: Enter Setup Mode

1. Press and hold the board's **BOOT** button.
2. While still holding it, power on the board (plug it in, or press its
   reset button if it's already powered).
3. Keep holding **BOOT** for a couple of seconds after power comes on —
   about 3 seconds total is a safe margin. Then release it.

If BOOT was held through that window, the board skips normal startup and
starts its own WiFi network instead of joining yours.
```

with:

```markdown
## Step 1: Enter Setup Mode

1. Make sure the board is already powered on and running normally — don't
   hold BOOT while plugging it in or resetting it.
2. Press and hold the board's **BOOT** button for about 3 seconds, then
   release it.

Releasing BOOT after that hold reboots the board into setup mode: it skips
normal startup and starts its own WiFi network instead of joining yours.
```

- [ ] **Step 2: Update "Changing Settings Later"**

Replace:

```markdown
You can re-run this whole process at any time — e.g. to move a board to a
new WiFi network, or fix a duplicate node ID — by repeating Step 1. Holding
BOOT through power-on always re-enters setup mode, even on a board that's
already configured and working.
```

with:

```markdown
You can re-run this whole process at any time — e.g. to move a board to a
new WiFi network, or fix a duplicate node ID — by repeating Step 1. Holding
BOOT for 3 seconds during normal operation, then releasing it, always
re-enters setup mode, even on a board that's already configured and
working.
```

- [ ] **Step 3: Update the troubleshooting bullet**

Replace:

```markdown
- **Don't see `Tortoise-Setup-XXXX` in your WiFi list:** the BOOT hold
  probably wasn't caught. Power off, hold BOOT, power back on, and hold for
  a full 3 seconds before releasing, then check again.
```

with:

```markdown
- **Don't see `Tortoise-Setup-XXXX` in your WiFi list:** the hold probably
  wasn't caught, or was released too early. Make sure the board is fully
  powered on and running first, then press and hold BOOT for a full 3
  seconds and release it — check again after it reboots.
```

- [ ] **Step 4: Rewrite `docs/software-class-list.md`'s "Entering Setup Mode" section**

Replace:

```markdown
### Entering Setup Mode

Hold **BOOT** while powering on. The node skips normal startup and instead:
```

with:

```markdown
### Entering Setup Mode

Press and hold **BOOT** for about 3 seconds during normal operation, then
release it — not while powering on. (GPIO0, the BOOT pin, is also the
ESP32's own boot-strapping pin: holding it low through a power-on or reset
puts the chip's ROM bootloader into permanent UART download mode before any
application code runs, so this gesture can only be detected live, after the
board has already booted normally — the same reason
`ButtonIdentifyRequestTrigger`'s short-press already works this way.)
Releasing after a qualifying hold reboots the node, which then skips normal
startup and instead:
```

Immediately after the existing numbered list (step 4, "On submit, the node validates..."), insert a new paragraph before the "If several new boards..." paragraph:

```markdown
A factory-fresh board with no valid config yet (`BootMode::NeedsCommissioning`)
still watches for this same hold-and-release gesture — it's the only way
such a board reaches wireless setup, since it has no `ControllerNode`
running yet either.
```

- [ ] **Step 5: Update the "New Classes" table**

Replace the `ButtonSetupModeTrigger` row:

```markdown
| `ButtonSetupModeTrigger` | Adapter | Reads the BOOT pin during `ControllerNode`'s construction |
```

with:

```markdown
| `ButtonSetupModeTrigger` | Adapter | Polls the BOOT pin during normal runtime (`loop()`), firing when a hold of at least the configured duration is released |
| `SetupModeRequestStore` | Port | `void requestOnNextBoot()` / `bool consumeRequest()` — persists "enter wireless setup" across the reboot the runtime hold-and-release triggers |
| `NvsSetupModeRequestStore` | Adapter | Persists the pending-setup flag in NVS (`Preferences`), in a namespace separate from `NodeConfig` |
```

- [ ] **Step 6: Update the Field Identification parenthetical**

Replace:

```markdown
A **short press** of BOOT during normal operation (not held through
power-up, so it doesn't trigger setup mode) makes the status LED blink the
```

with:

```markdown
A **short press** of BOOT during normal operation (shorter than the
setup-mode hold, so it doesn't trigger setup mode) makes the status LED
blink the
```

- [ ] **Step 7: Add a completed row to `docs/task-status.md`**

Look up the real commit hashes from Tasks 1-4 with `git log --oneline` (do not guess). Add a row to the Completed table (after the "Wireless setup mode boot logic" row):

```markdown
| Wireless setup runtime trigger fix | ✅ Done | Commits `<task1-hash>` (`SetupModeRequestStore`/`FakeSetupModeRequestStore`), `<task2-hash>` (`NvsSetupModeRequestStore`), `<task3-hash>` (redefine `ButtonSetupModeTrigger` for runtime hold-and-release), `<task4-hash>` (wire into `src/main.cpp`). Fixes a hardware bug found by on-device testing: holding BOOT through power-on collides with the ESP32's own GPIO0 boot-strapping pin and puts the ROM into permanent UART download mode instead of ever running the app. Wireless setup is now entered by holding BOOT for 3s *during* normal runtime and releasing it, detected the same way `ButtonIdentifyRequestTrigger`'s short-press already was; the request is persisted via `NvsSetupModeRequestStore` across the `ESP.restart()` this triggers. Also removes the previous design's fixed ~2s delay on every boot, since boot-time button polling is gone. |
```

- [ ] **Step 8: Fix the stale "Known scaffolding debt" bullet**

Replace:

```markdown
- A board in `BootMode::NeedsCommissioning` (invalid config, BOOT not held)
  currently gives no visual signal — it silently runs only the serial
```

with:

```markdown
- A board in `BootMode::NeedsCommissioning` (invalid config, no
  wireless-setup hold detected) currently gives no visual signal — it
  silently runs only the serial
```

- [ ] **Step 9: Commit**

Commit via the `/arlo-commits` skill.

---

### Task 6: On-device verification

**Files:** none (verification only).

**Interfaces:** none.

- [ ] **Step 1: Build and flash**

Run: `pio run -e esp32dev --target upload --upload-port COM3`
Expected: `SUCCESS`.

- [ ] **Step 2: Start the serial monitor**

Run: `pio device monitor -e esp32dev --port COM3`

- [ ] **Step 3: Verify normal boot is unaffected**

Let the board boot without touching BOOT. Expected: no `DOWNLOAD_BOOT`/`waiting for download` in the log — the app runs (bench-serial `Preferences` lines or later application output appear, not just ROM banner text).

- [ ] **Step 4: Verify the new gesture enters wireless setup**

With the board already running normally, press and hold BOOT for about 3 seconds, then release. Expected: the board reboots (visible in the serial log as a fresh ROM banner with `boot:0x13 (SPI_FAST_FLASH_BOOT)`, *not* `DOWNLOAD_BOOT`), and the status LED (GPIO 2) begins a rapid steady blink. Confirm a `Tortoise-Setup-XXXX` WiFi network is visible from a phone/laptop.

- [ ] **Step 5: Verify a short press still does field-identify, not setup**

Once back in normal operation with a valid config, a short tap of BOOT (well under 1.5s) should blink out the node's id pattern, not trigger a reboot into setup mode.

- [ ] **Step 6: Report results**

No commit — this task is a manual check. If any expectation isn't met, treat it as a new bug and return to Phase 1 of systematic-debugging rather than patching blind.
