# Bench Serial Always-On Wiring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the already-built, already-tested bench serial commissioning classes (backlog #18: `SerialCommissioningAdapter`, `EspUartPort`, `CommissioningSession`) into `src/main.cpp` for real, so a technician can plug a board into USB and commission it (`id`/`wifi`/`broker`/`turnout`/`show`/`save`/`reboot`) at any time — including while the board is already running normally.

**Architecture:** No new domain/port/adapter classes. This is composition-root wiring only, in `src/main.cpp`. Per `docs/software-class-list.md`'s own description of the bench workflow ("Plug into USB, open a serial terminal") — it's meant to work regardless of whether the node is mid-normal-operation or freshly booted, since it's a distinct physical channel (UART) from the turnout GPIO/MQTT graph `ControllerNode` owns. That means **no boot-mode-selection logic is needed here** — the serial commissioning adapter runs in parallel with `ControllerNode`, polled every `loop()` iteration alongside `node->tick()`.

**Tech Stack:** PlatformIO, `esp32dev` environment. No new `lib_deps`.

## Global Constraints

- `src/main.cpp` is the composition root only — no business logic. This plan only adds object construction/wiring to it.
- No `delay()` in `loop()`. `SerialCommissioningAdapter::poll()` and `ControllerNode::tick()` are both already non-blocking.
- No dynamic allocation after boot — all new objects are function-local statics inside `setup()`, same pattern the existing `ControllerNode` instance already uses.
- `src/main.cpp` cannot be native-unit-tested (`test_build_src = false` in `platformio.ini` — native binaries never compile `src/`). Verify with `pio run -e esp32dev` directly — this *is* the real, permanent target file, so unlike prior build-check tasks for standalone adapter classes, there is nothing to temporarily wire-then-revert here. The change itself is the deliverable.
- ACN notation for the commit message. Never `--amend`, never `--no-verify`.
- **This closes a piece of existing scaffolding debt as a side effect, not something to separately implement:** `docs/task-status.md`'s "Known scaffolding debt" notes that `EspUartPort.h` has had zero ongoing build-check coverage since backlog #18's temporary wiring was reverted (`lib_ldf_mode = deep+` doesn't force-compile a header nothing `#include`s). Once this plan's `src/main.cpp` change lands, `EspUartPort.h` is genuinely, permanently compiled by every `pio run -e esp32dev` from now on. Task 2 removes that debt bullet from the docs.

---

### Task 1: Wire `EspUartPort`/`SerialCommissioningAdapter` into `src/main.cpp`

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `EspUartPort` (existing, `lib/McsCore/src/adapters/EspUartPort.h` — `explicit EspUartPort(unsigned long baudRate)`), `NvsConfigStore` (existing, `lib/McsCore/src/adapters/NvsConfigStore.h` — default-constructible, persists to the fixed `"mcs-cfg"` NVS namespace), `CommissioningSession` (existing, `lib/McsCore/src/domain/CommissioningSession.h` — `explicit CommissioningSession(ConfigStore&)`), `SerialCommissioningAdapter` (existing, `lib/McsCore/src/adapters/SerialCommissioningAdapter.h` — `SerialCommissioningAdapter(UartPort&, CommissioningSession&)`, `void poll()`, `bool rebootRequested() const`).
- Produces: nothing consumed by a later task — this is the composition root, the end of the dependency chain.

**Why a second `NvsConfigStore` instance, not reusing `ControllerNode`'s:** `NvsConfigStore` is stateless per call (`save()`/`load()` each open and close `Preferences` fresh) — a second instance reads/writes the exact same persisted `"mcs-cfg"` NVS namespace as `ControllerNode`'s own. This avoids adding a public accessor to `ControllerNode` just to reach its private `configStore_`, and keeps the two composition-root concerns (normal operation vs. commissioning) decoupled. `CommissioningSession` seeds its draft from `store.load()` once at construction (same as bench serial commissioning always has) — a technician's `save()` during a running session persists to NVS immediately, but per the design doc's "Why `reboot`, Not Live-Apply" section, `ControllerNode`'s already-running object graph does **not** pick up the change until an actual `reboot`, which is exactly what `SerialCommissioningAdapter::rebootRequested()` triggers via `ESP.restart()`.

- [ ] **Step 1: Read the current file**

Read `src/main.cpp` in full and confirm its exact current content matches what's described below (it should be unchanged since backlog #16 — a `ControllerNode* node` function-local-static pattern with `setup()`/`loop()`). If it has drifted from this description, stop and report rather than guessing.

- [ ] **Step 2: Make the change**

Replace the full contents of `src/main.cpp` with:

```cpp
#include <Arduino.h>

#include "adapters/ControllerNode.h"
#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "domain/CommissioningSession.h"

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
    static ControllerNode instance;
    node = &instance;
    node->begin();

    // Bench serial commissioning runs in parallel with normal operation, not
    // as an alternate boot mode - it's a distinct physical channel (UART)
    // from the turnout GPIO/MQTT graph ControllerNode owns, so a technician
    // can plug in and commission at any time, per docs/software-class-list.md's
    // "Plug into USB, open a serial terminal" workflow. Uses its own
    // NvsConfigStore instance (stateless per call, same "mcs-cfg" namespace
    // ControllerNode's own store reads) rather than reaching into
    // ControllerNode's internals.
    static NvsConfigStore commissioningStore;
    static CommissioningSession commissioningSession(commissioningStore);
    static EspUartPort uart(115200);
    static SerialCommissioningAdapter adapter(uart, commissioningSession);
    commissioningAdapter = &adapter;
}

void loop()
{
    node->tick();

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
! F Wire bench serial commissioning into main.cpp

SerialCommissioningAdapter now runs in parallel with ControllerNode's
normal tick loop, polled every loop() iteration - not an alternate
boot mode, since UART doesn't conflict with the turnout GPIO/MQTT
graph. A technician can commission (id/wifi/broker/turnout/show/
save/reboot) at any time by plugging in USB, per the bench workflow
in docs/software-class-list.md. Build-check verified only
(pio run -e esp32dev) - main.cpp has no native equivalent.

EOF
)"
```

---

### Task 2: Update `docs/task-status.md`

**Files:**
- Modify: `docs/task-status.md`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing (docs only).

- [ ] **Step 1: Update the Completed table**

Add a row documenting this wiring, citing the real commit hash from Task 1 (look it up with `git log --oneline` — do not guess). Follow the existing row style.

- [ ] **Step 2: Remove the now-resolved `EspUartPort` coverage-gap bullet**

In "Known scaffolding debt", remove the bullet describing `EspUartPort.h`'s lack of ongoing build-check coverage (currently reads "`lib/McsCore/src/adapters/EspUartPort.h` (task #18) has no ongoing build-check coverage..."). It's resolved: `src/main.cpp` now permanently `#include`s it, so every `pio run -e esp32dev` compiles it for real.

- [ ] **Step 3: Add a note about the always-on serial channel to the two "not yet wired" bullets**

The existing bullets for task #19 (`ButtonSetupModeTrigger`/`WebFormCommissioningAdapter`/`EspDeviceIdentity`/`CaptivePortalServer` not wired) and task #20 (`ButtonIdentifyRequestTrigger`/`BlinkOutIdentifier`/`NodeIdCollisionGuard`/`MqttNodePresenceReporter` not wired) are unaffected by this change and should be left as-is — this plan only wires the bench serial path, not wireless setup or field identification.

- [ ] **Step 4: Commit**

```bash
git add docs/task-status.md
git commit -m "$(cat <<'EOF'
. d Mark bench serial always-on wiring complete in task-status.md

EOF
)"
```
