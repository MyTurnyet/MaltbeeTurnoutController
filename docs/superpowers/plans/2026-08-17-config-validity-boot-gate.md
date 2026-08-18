# Config-Validity Boot Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the scaffolding-debt gap where `ControllerNode` constructs `EspDigitalOutput`/`EspDigitalInput` against a factory-default (or otherwise invalid) `NodeConfig`'s sentinel pin `-1`, untested and unsafe. Gate hardware-graph construction on `NodeConfig::validate()` first.

**Architecture:** One new tiny pure domain class (`BootModeSelector`, plus a `BootMode` enum) makes the "is this config safe to run normally" decision testable in isolation. `src/main.cpp` uses it to decide whether to construct `ControllerNode` at all.

**Approved product decision for this plan's scope (confirmed by the project owner):** when config is invalid at boot, do **not** construct `ControllerNode`'s hardware graph. Bench serial commissioning (already wired, backlog #21) stays active regardless, so a technician can always recover the board via USB. This plan does **not** add wireless setup mode (BOOT-hold) or a visual "needs commissioning" LED indicator — those are backlog #23 (blocked on this plan) and backlog #25 (needs a status-LED GPIO pin decision first) respectively. A board with invalid config and BOOT not held simply runs the always-on serial channel with no turnout/MQTT activity and no LED signal yet — silent-but-safe, not silent-but-broken.

**Tech Stack:** PlatformIO, Catch2 3.7.1 (native test for the new domain class), `esp32dev` build-check for the `main.cpp` change (no native equivalent for the composition root).

## Global Constraints

- Domain code must compile and run under `native` without `Arduino.h`. `BootMode`/`BootModeSelector` are pure — no guard needed, same as `TopicScheme`/`SetupApName`/other static-method pure classes.
- No `delay()`, no dynamic allocation after boot.
- ACN notation for every commit message. Never `--amend`, never `--no-verify`.
- `src/main.cpp` has no native equivalent — verify with `pio run -e esp32dev` directly on the real file, same as backlog #21 (no temporary-wire-then-revert dance; this *is* the permanent change).
- **Read `src/main.cpp` fresh before editing it** — it was just changed by backlog #21 (commit `5df0be8`, merged to `main` in `5af43c3`). Do not work from the version described in older plans in this repo; confirm the current real content first.

---

### Task 1: `BootMode` + `BootModeSelector` domain classes

**Files:**
- Create: `lib/McsCore/src/domain/BootMode.h`
- Create: `lib/McsCore/src/domain/BootModeSelector.h`
- Test: `test/test_boot_mode_selector/test_main.cpp`

**Interfaces:**
- Consumes: `NodeConfig` (existing, `lib/McsCore/src/domain/NodeConfig.h` — `std::vector<ConfigError> validate() const`, `static NodeConfig factoryDefault()`, `withId`/`withTurnout` etc. for building valid test fixtures).
- Produces: `BootMode` enum, `BootModeSelector::select(const NodeConfig&) -> BootMode` — consumed by Task 2's `src/main.cpp` wiring.

A one-method pure static utility, same style as `SetupApName`/`TopicScheme`: wraps `NodeConfig::validate().empty()` behind a named, testable decision rather than leaving that check as inline logic in the composition root.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_boot_mode_selector/test_main.cpp
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

TEST_CASE("BootModeSelector selects Normal for a fully valid config")
{
    REQUIRE(BootModeSelector::select(fullyValidConfig()) == BootMode::Normal);
}

TEST_CASE("BootModeSelector selects NeedsCommissioning for the factory default")
{
    REQUIRE(BootModeSelector::select(NodeConfig::factoryDefault()) == BootMode::NeedsCommissioning);
}

TEST_CASE("BootModeSelector selects NeedsCommissioning for an out-of-range node id alone")
{
    NodeConfig config = fullyValidConfig().withId(NodeId(0));
    REQUIRE(BootModeSelector::select(config) == BootMode::NeedsCommissioning);
}

TEST_CASE("BootModeSelector selects NeedsCommissioning for a pin conflict alone")
{
    Orientation orientation = Orientation::normal();
    Duration settle(50);
    Duration timeout(200);
    NodeConfig config = fullyValidConfig().withTurnout(1, TurnoutConfig(TurnoutId(2), 10, 11, orientation, settle, timeout));
    REQUIRE(BootModeSelector::select(config) == BootMode::NeedsCommissioning);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_boot_mode_selector`
Expected: FAIL to compile — neither header exists yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/McsCore/src/domain/BootMode.h
#pragma once

enum class BootMode
{
    Normal,
    NeedsCommissioning
};
```

```cpp
// lib/McsCore/src/domain/BootModeSelector.h
#pragma once

#include "domain/BootMode.h"
#include "domain/NodeConfig.h"

class BootModeSelector
{
public:
    static BootMode select(const NodeConfig& config)
    {
        return config.validate().empty() ? BootMode::Normal : BootMode::NeedsCommissioning;
    }
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_boot_mode_selector`
Expected: PASS, 4 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/domain/BootMode.h lib/McsCore/src/domain/BootModeSelector.h \
        test/test_boot_mode_selector/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add BootMode/BootModeSelector domain classes

EOF
)"
```

---

### Task 2: Gate `ControllerNode` construction on `BootModeSelector` in `src/main.cpp`

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `BootModeSelector`/`BootMode` (Task 1), everything backlog #21 already wired (`NvsConfigStore`, `CommissioningSession`, `EspUartPort`, `SerialCommissioningAdapter`), `ControllerNode` (existing), `NodeConfig` (existing).
- Produces: nothing consumed by a later task — composition root, end of the chain.

- [ ] **Step 1: Read the current file**

Read `src/main.cpp` in full first (see the Global Constraints note above — it changed under backlog #21). Confirm it currently has: a `static ControllerNode* node = nullptr;`, a `static SerialCommissioningAdapter* commissioningAdapter = nullptr;`, a `setup()` that unconditionally constructs `ControllerNode` and the bench-serial-commissioning objects, and a `loop()` that unconditionally calls `node->tick()` then polls the commissioning adapter. If it doesn't match this description, stop and report rather than guessing.

- [ ] **Step 2: Make the change**

Replace the full contents of `src/main.cpp` with:

```cpp
#include <Arduino.h>

#include "adapters/ControllerNode.h"
#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "domain/CommissioningSession.h"
#include "domain/NodeConfig.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"

// ControllerNode is constructed here (function-local static, not file-scope)
// because its constructor performs real NVS/GPIO work via NvsConfigStore and
// the Esp*Output/Input adapters. A file-scope static would run before
// app_main() calls initArduino() (see cores/esp32/main.cpp), which is the
// only place nvs_flash_init() runs (esp32-hal-misc.c) - constructing before
// that silently drops all saved NVS config to factory defaults. setup() only
// runs after initArduino(), so this ordering is safe.
static ControllerNode* node = nullptr;
static SerialCommissioningAdapter* commissioningAdapter = nullptr;

void setup()
{
    // Bench serial commissioning runs in parallel with normal operation
    // regardless of config validity, so a technician can always recover a
    // board stuck in NeedsCommissioning mode by plugging in USB - it's a
    // distinct physical channel (UART) from the turnout GPIO/MQTT graph.
    static NvsConfigStore commissioningStore;
    static CommissioningSession commissioningSession(commissioningStore);
    static EspUartPort uart(115200);
    static SerialCommissioningAdapter adapter(uart, commissioningSession);
    commissioningAdapter = &adapter;

    // Constructing ControllerNode wires real GPIO pins from NodeConfig - on a
    // factory-default or otherwise invalid config (e.g. NodeId(0), sentinel
    // pin -1 on every turnout), that would build EspDigitalOutput/
    // EspDigitalInput against pin -1, which is untested and unsafe. Check
    // validity first and skip building the hardware graph entirely if it
    // would be unsafe - node stays nullptr, loop() only runs the always-on
    // serial commissioning channel until a technician saves a valid config
    // and reboots.
    NodeConfig config = commissioningStore.load();
    if (BootModeSelector::select(config) == BootMode::Normal)
    {
        static ControllerNode instance;
        node = &instance;
        node->begin();
    }
}

void loop()
{
    if (node != nullptr)
    {
        node->tick();
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
! F Gate ControllerNode construction on config validity

On a factory-default or otherwise invalid NodeConfig, main.cpp no
longer constructs ControllerNode's hardware graph at all - avoids
wiring EspDigitalOutput/EspDigitalInput against the sentinel pin -1.
Bench serial commissioning stays active either way, so an invalid
board can always be recovered over USB. Build-check verified only
(pio run -e esp32dev) - main.cpp has no native equivalent.

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

Add a row documenting this gate, citing the real commit hashes from Tasks 1-2 (look them up with `git log --oneline` — do not guess).

- [ ] **Step 2: Remove the now-resolved pin-`-1` scaffolding-debt bullet**

In "Known scaffolding debt", remove the bullet reading "`ControllerNode` assumes an already-commissioned `NodeConfig`; on a factory-default board it constructs adapters against pin `-1`, which is untested — not reachable without bench commissioning (task #18) writing real values first." It's resolved: `ControllerNode` is no longer constructed at all when config is invalid.

- [ ] **Step 3: Add a new scaffolding-debt bullet for the deferred visual indicator**

Add a bullet noting that a board in `BootMode::NeedsCommissioning` (invalid config, BOOT not held) currently gives no visual signal — it silently runs only the serial commissioning channel with no turnout/MQTT activity. A distinct LED blink pattern is deferred to backlog #25, which needs a status-LED GPIO pin decision first. Note this was an explicit, approved scope boundary for this plan, not an oversight.

- [ ] **Step 4: Commit**

```bash
git add docs/task-status.md
git commit -m "$(cat <<'EOF'
. d Mark config-validity boot gate complete in task-status.md

EOF
)"
```
