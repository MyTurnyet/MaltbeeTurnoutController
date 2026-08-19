# MQTT Turnout-Command Activity LED Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Flash the status LED (GPIO 2) three times, quickly, each time a node successfully decodes and acts on an incoming MQTT turnout command — lets a customer/builder confirm at the board that JMRI's commands are actually reaching it, without a laptop or serial connection.

**Architecture:** A new pure domain class, `FlashBurst`, produces a one-shot N-flash on/off pattern (same shape as the existing `BlinkOutIdentifier`/`SteadyBlinker`, but non-repeating). `MqttCommandSource` gains an edge-triggered "a command was just decoded and dispatched" flag with read-and-clear access (`consumeReceived()`), set only on successful decode — malformed payloads and non-command traffic (e.g. this node's own presence-topic messages) don't count. `ControllerNode` exposes a one-line passthrough. `src/main.cpp` polls that passthrough once per `loop()` tick, arms a `Deadline` (reusing the existing class, same idiom as the identify-blink window) on a hit, and extends the existing collision-blink → identify-blink → off priority chain with a third tier: MQTT flash-burst, shown only when neither collision- nor identify-blink is active.

**Tech Stack:** PlatformIO, Catch2 3.7.1 (native test for `FlashBurst`), `esp32dev` build-check + on-device verification for `MqttCommandSource`/`ControllerNode`/`src/main.cpp` (all `#ifdef ARDUINO`-gated or the composition root, no native equivalent).

## Global Constraints

- Domain code (`FlashBurst`) must compile and run under `native` without `Arduino.h` — pure `Duration`/`Level` math, no `Clock`, no hardware.
- No `delay()`, no dynamic allocation after boot.
- Commit via the `/arlo-commits` skill (per this repo's `CLAUDE.md`) — apply its classification methodology and commit directly rather than stalling on its interactive-approval step when dispatched to a non-interactive subagent (established practice in this repo — see prior plans).
- ACN 8-LoC rule: any `F`/`B` commit diff over 8 lines is risk `!`, regardless of test coverage.
- `MqttCommandSource`, `ControllerNode`, and `src/main.cpp` are all `#ifdef ARDUINO`-gated or the composition root — none have a native test equivalent. Verify those tasks with `pio run -e esp32dev` build-check only, matching this repo's existing precedent (e.g. `NvsConfigStore`, `MqttLink`).
- **Which messages count (already decided, not open for reinterpretation):** only turnout commands that `PayloadCodec::decode` successfully parses and that reach `sink_.command(...)`. This node's own presence-topic traffic and malformed/undecodable payloads never flash the LED.
- **LED priority (already decided):** collision-blink (steady fast, `ControllerNode::blocked()`) → identify-blink (per-id pattern, 5s after a short BOOT press) → MQTT flash-burst (new) → off. The flash-burst only ever shows in the "off" slot — it never interrupts collision- or identify-blink, and a command received while one of those is active is simply not shown (no queuing).
- **Flash pattern (already decided):** 80ms on / 80ms off × 3 flashes (480ms total). A new command arriving mid-burst restarts the burst from flash 1.
- Full design context: `docs/superpowers/specs/2026-08-18-mqtt-activity-led-design.md`.

---

### Task 1: `FlashBurst` domain class (TDD)

**Files:**
- Create: `lib/McsCore/src/domain/FlashBurst.h`
- Create: `test/test_flash_burst/test_main.cpp`

**Interfaces:**
- Consumes: `Duration`, `Level` (existing).
- Produces: `FlashBurst(Duration onDuration, Duration offDuration, int flashCount)`, `Level levelAt(Duration elapsed) const` — consumed by Task 3's `src/main.cpp` wiring.

- [ ] **Step 1: Write the failing test**

Create `test/test_flash_burst/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/FlashBurst.h"

TEST_CASE("FlashBurst is on immediately at elapsed zero")
{
    FlashBurst burst(Duration(100), Duration(50), 3);
    REQUIRE(burst.levelAt(Duration(0)) == Level::High);
}

TEST_CASE("FlashBurst turns off after onDuration within the first flash")
{
    FlashBurst burst(Duration(100), Duration(50), 3);
    REQUIRE(burst.levelAt(Duration(100)) == Level::Low);
    REQUIRE(burst.levelAt(Duration(149)) == Level::Low);
}

TEST_CASE("FlashBurst turns on again for the second flash")
{
    FlashBurst burst(Duration(100), Duration(50), 3);
    REQUIRE(burst.levelAt(Duration(150)) == Level::High);
    REQUIRE(burst.levelAt(Duration(249)) == Level::High);
}

TEST_CASE("FlashBurst produces exactly flashCount on/off cycles")
{
    // cycle = 150ms. Flash 3 (last): [300,400)=High, [400,450)=Low.
    FlashBurst burst(Duration(100), Duration(50), 3);
    REQUIRE(burst.levelAt(Duration(300)) == Level::High);
    REQUIRE(burst.levelAt(Duration(399)) == Level::High);
    REQUIRE(burst.levelAt(Duration(400)) == Level::Low);
    REQUIRE(burst.levelAt(Duration(449)) == Level::Low);
}

TEST_CASE("FlashBurst stays Low at and after the total burst duration, and never repeats")
{
    // total = 3 * 150ms = 450ms.
    FlashBurst burst(Duration(100), Duration(50), 3);
    REQUIRE(burst.levelAt(Duration(450)) == Level::Low);
    REQUIRE(burst.levelAt(Duration(1000)) == Level::Low);
    REQUIRE(burst.levelAt(Duration(100000)) == Level::Low);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_flash_burst`
Expected: FAIL to compile (`FlashBurst.h` doesn't exist yet).

- [ ] **Step 3: Write the implementation**

Create `lib/McsCore/src/domain/FlashBurst.h`:

```cpp
#pragma once

#include "domain/Duration.h"
#include "domain/Level.h"

// One-shot burst of flashCount on/off blinks, then stays dark forever -
// unlike BlinkOutIdentifier, there is no pause-and-repeat. A fresh trigger
// (an external Deadline re-arm, driven by the composition root) is what
// restarts it, not levelAt() looping on its own.
class FlashBurst
{
public:
    FlashBurst(Duration onDuration, Duration offDuration, int flashCount)
        : onDuration_(onDuration), offDuration_(offDuration), flashCount_(flashCount)
    {
    }

    Level levelAt(Duration elapsed) const
    {
        unsigned long cycleMs = onDuration_.milliseconds() + offDuration_.milliseconds();
        unsigned long totalMs = cycleMs * static_cast<unsigned long>(flashCount_);
        unsigned long t = elapsed.milliseconds();

        if (t >= totalMs)
        {
            return Level::Low;
        }

        unsigned long withinCycle = t % cycleMs;
        return withinCycle < onDuration_.milliseconds() ? Level::High : Level::Low;
    }

private:
    Duration onDuration_;
    Duration offDuration_;
    int flashCount_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_flash_burst`
Expected: PASS, 5 test cases.

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: all binaries pass — `FlashBurst` has no other callers at the source level yet (Task 3 adds the only one, in `main.cpp`, which isn't part of the native build).

- [ ] **Step 6: Commit**

Commit via the `/arlo-commits` skill.

---

### Task 2: Expose the turnout-command-received signal

**Files:**
- Modify: `lib/McsCore/src/adapters/MqttCommandSource.h`
- Modify: `lib/McsCore/src/adapters/ControllerNode.h`

**Interfaces:**
- Consumes: nothing new.
- Produces: `MqttCommandSource::consumeReceived() -> bool`, `ControllerNode::turnoutCommandReceived() -> bool` — consumed by Task 3's `src/main.cpp` wiring.

Both files are `#ifdef ARDUINO`-gated with no native test today (per Global Constraints) — verify with `pio run -e esp32dev` only. These two changes are bundled into one task because they're a single, inseparable signal path: the `ControllerNode` passthrough is meaningless without `MqttCommandSource`'s flag, and neither is independently reviewable.

- [ ] **Step 1: Read both current files**

Read `lib/McsCore/src/adapters/MqttCommandSource.h` and `lib/McsCore/src/adapters/ControllerNode.h` in full first. Confirm `MqttCommandSource::handle` currently looks like:

```cpp
void handle(TurnoutId id, const std::string& payload)
{
    std::optional<TurnoutPosition> position = PayloadCodec::decode(payload);
    if (!position.has_value())
    {
        return;
    }

    sink_.command(id, *position);
}
```

and that `ControllerNode` currently has no method resembling `turnoutCommandReceived`. If either doesn't match, stop and report rather than guessing.

- [ ] **Step 2: Modify `MqttCommandSource.h`**

Replace the full contents of `lib/McsCore/src/adapters/MqttCommandSource.h` with:

```cpp
#pragma once

#ifdef ARDUINO

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
    }

    void subscribeAll(int nodeId)
    {
        for (int channel = 1; channel <= TurnoutRegistry::TurnoutsPerNode; ++channel)
        {
            TurnoutId id(nodeId * 100 + channel);
            std::string topic = TopicScheme::topicFor(id);
            link_.subscribe(topic, [this, id](const std::string& payload) {
                handle(id, payload);
            });
        }
    }

    // Edge-triggered: true if a turnout command was successfully decoded
    // and dispatched since the last call, then clears back to false. Lets
    // src/main.cpp drive an LED flash without polling per-turnout state.
    bool consumeReceived()
    {
        bool wasReceived = receivedThisTick_;
        receivedThisTick_ = false;
        return wasReceived;
    }

private:
    void handle(TurnoutId id, const std::string& payload)
    {
        std::optional<TurnoutPosition> position = PayloadCodec::decode(payload);
        if (!position.has_value())
        {
            return;
        }

        sink_.command(id, *position);
        receivedThisTick_ = true;
    }

    MqttLink& link_;
    TurnoutCommandSink& sink_;
    bool receivedThisTick_ = false;
};

#endif
```

- [ ] **Step 3: Modify `ControllerNode.h`**

In `lib/McsCore/src/adapters/ControllerNode.h`, add a new public method immediately after `blocked()`:

```cpp
    // True if a turnout command was successfully decoded and dispatched
    // since the last call to this method (edge-triggered, read-and-clear).
    // src/main.cpp calls this once per loop() tick, right after tick(), to
    // drive the MQTT-activity flash-burst on the status LED.
    bool turnoutCommandReceived()
    {
        return commandSource_.consumeReceived();
    }
```

- [ ] **Step 4: Build-check**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

Commit via the `/arlo-commits` skill.

---

### Task 3: Wire the flash-burst into `src/main.cpp`

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `FlashBurst` (Task 1), `MqttCommandSource`/`ControllerNode::turnoutCommandReceived()` (Task 2), everything else already wired.
- Produces: nothing consumed by a later task — composition root, end of the chain.

- [ ] **Step 1: Read the current file**

Read `src/main.cpp` in full first. Confirm its current shape matches: `loop()`'s `node != nullptr` / `!node->blocked()` branch polls `identifyTrigger`, arms `identifyDeadline` on a qualifying press, then does
`statusLed->write(identifying ? blinkIdentifier->levelAt(now - identifyStart) : Level::Low);`
If it doesn't match, stop and report rather than guessing.

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
#include "domain/FlashBurst.h"
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

// Timing for the MQTT turnout-command-received flash burst - three quick
// flashes, lowest priority on the shared status LED (only shown when
// neither collision- nor identify-blink is active). See
// docs/superpowers/specs/2026-08-18-mqtt-activity-led-design.md.
constexpr unsigned long kMqttFlashOnMs = 80;
constexpr unsigned long kMqttFlashOffMs = 80;
constexpr int kMqttFlashCount = 3;
constexpr unsigned long kMqttFlashBurstTotalMs = (kMqttFlashOnMs + kMqttFlashOffMs) * kMqttFlashCount;
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
static FlashBurst* mqttFlashBurst = nullptr;
static ButtonSetupModeTrigger* runtimeSetupTrigger = nullptr;
static NvsSetupModeRequestStore* setupRequestStore = nullptr;
static Deadline identifyDeadline;
static Instant identifyStart(0);
static Deadline mqttActivityDeadline;
static Instant mqttActivityStart(0);

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

            // Field identification (short-press BOOT blinks the node's id),
            // the collision-error pattern (steady fast blink, driven
            // instead whenever ControllerNode::blocked() is true), and the
            // MQTT turnout-command flash-burst (lowest priority - only
            // shown when neither of the above is active) share the status
            // LED - only meaningful once a node has an actual id and is
            // subscribed to turnout commands, so these are only
            // constructed in Normal mode.
            static ButtonIdentifyRequestTrigger trigger(bootPin, Duration(kIdentifyMinPressMs), Duration(kIdentifyMaxPressMs));
            identifyTrigger = &trigger;
            static BlinkOutIdentifier identifier(config.id(), Duration(200), Duration(200), Duration(1000));
            blinkIdentifier = &identifier;
            static SteadyBlinker errorBlinker{Duration(kCollisionBlinkHalfPeriodMs)};
            collisionBlinker = &errorBlinker;
            static FlashBurst flasher(Duration(kMqttFlashOnMs), Duration(kMqttFlashOffMs), kMqttFlashCount);
            mqttFlashBurst = &flasher;
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

            if (node->turnoutCommandReceived())
            {
                mqttActivityDeadline.arm(now, Duration(kMqttFlashBurstTotalMs));
                mqttActivityStart = now;
            }

            bool identifying = identifyDeadline.armed() && !identifyDeadline.expired(now);
            bool flashingMqtt = mqttActivityDeadline.armed() && !mqttActivityDeadline.expired(now);

            Level level;
            if (identifying)
            {
                level = blinkIdentifier->levelAt(now - identifyStart);
            }
            else if (flashingMqtt)
            {
                level = mqttFlashBurst->levelAt(now - mqttActivityStart);
            }
            else
            {
                level = Level::Low;
            }
            statusLed->write(level);
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

### Task 4: Update docs

**Files:**
- Modify: `docs/first-time-setup.md`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing (docs only).

- [ ] **Step 1: Add a row to the Step 5 LED table**

In `docs/first-time-setup.md`, find the table under "Step 5: Confirm It's Running":

```markdown
| LED behavior | Meaning |
|---|---|
| Off | Normal — running quietly, nothing to report. |
| Blinks its node number (N short blinks, pause, repeat) | Someone short-pressed BOOT to ask "which board is this?" — see below. |
| Fast, steady blinking (no pauses) | **Duplicate node ID** — another board on the network already claims this ID. Re-enter setup mode (Step 1) and give this board a different ID. |
```

Add a new row after the last one:

```markdown
| Three quick flashes | Received a turnout command from JMRI — confirms MQTT commands are reaching this board. |
```

- [ ] **Step 2: Commit**

Commit via the `/arlo-commits` skill.

---

### Task 5: On-device verification

**Files:** none (verification only).

**Interfaces:** none.

- [ ] **Step 1: Build and flash**

Run: `pio run -e esp32dev --target upload --upload-port COM3`
Expected: `SUCCESS`.

- [ ] **Step 2: Start the serial monitor**

Run: `pio device monitor -e esp32dev --port COM3`

- [ ] **Step 3: Confirm normal operation is unaffected**

With a board that has a valid, previously-saved config, confirm it boots into `BootMode::Normal` as before (no `DOWNLOAD_BOOT`, LED off/idle) and that a short BOOT press still triggers the identify-blink pattern as before.

- [ ] **Step 4: Confirm the flash-burst fires on a real turnout command**

Publish a turnout command to this node's topic (e.g. `mosquitto_pub -h <broker> -t <topic> -m <payload>` matching `TopicScheme`/`PayloadCodec`'s expected format, or trigger it from JMRI if available). Expected: the status LED flashes three times quickly (~480ms total), then goes dark, matching `docs/first-time-setup.md`'s new table row.

- [ ] **Step 5: Confirm priority — flash-burst never interrupts identify-blink or collision-blink**

While the identify-blink pattern is active (short-press BOOT within 5s of the test), publish a turnout command. Expected: the identify pattern continues uninterrupted; the flash-burst does not visibly interject. (If a collision/duplicate-ID scenario is easy to reproduce, repeat with collision-blink active instead — otherwise this sub-check may be skipped, since the code path is identical in shape to the already-covered identify case.)

- [ ] **Step 6: Report results**

No commit — this task is a manual check. If any expectation isn't met, treat it as a new bug and return to Phase 1 of systematic-debugging rather than patching blind.
