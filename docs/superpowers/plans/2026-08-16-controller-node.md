# ControllerNode Composition Root Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `ControllerNode` (Build Order 12) and wire it into `src/main.cpp` for real, so the ESP32-WROOM-32 firmware is flashable and functional for the first time.

**Architecture:** `ControllerNode` is the single documented static-object exception to "no statics" — it constructs the whole object graph once (config → per-turnout `EspDigitalOutput`/`EspDigitalInput` → `WiFiLink`/`MqttLink` → `TurnoutRegistry` owning 8 `Turnout`s → `MqttCommandSource`/`MqttPositionReporter`) and exposes only `begin()`/`tick()`. `main.cpp` becomes a true composition root: one `ControllerNode` instance (a function-local static constructed inside `setup()` — see Task 1 Step 2's correction note for why not file-scope), `setup()` calls `begin()`, `loop()` calls `tick()` via a pointer to it, no other logic.

**Tech Stack:** PlatformIO, `esp32dev` env (C++17, `espressif32` platform, `arduino` framework), `knolleary/PubSubClient@^2.8`.

## Global Constraints

- Domain/application code must compile under `native` without `Arduino.h` — but `ControllerNode` is composition-root/adapter-layer by definition (it directly names `EspDigitalOutput`, `NvsConfigStore`, `WiFiLink`, etc.), so like every class built in backlog #15 it lives under `#ifdef ARDUINO` in `lib/McsCore/src/adapters/` and has **no native test**. Verification is the same **build-check cycle** used throughout #15: `pio run -e esp32dev` must compile and link. Unlike #15's adapters, there is nothing to revert afterward — wiring `main.cpp` for real *is* this plan's deliverable.
- No `delay()` anywhere in the resulting system (`ControllerNode::tick()` only calls non-blocking `poll()`/`tick()` methods already built in #15).
- No dynamic allocation after boot — every member of `ControllerNode` is a fixed-size value/array embedded directly in the object (verified member-by-member in Task 1 below); the one `static ControllerNode` in `main.cpp` is the sole documented static.
- Commit via the `/arlo-commits` skill process (per `CLAUDE.md`) — survey, group, classify, get explicit user approval, then execute. Never hand-write `git commit` messages outside that process.
- `native` test suite (25 binaries) must remain green throughout — nothing in this plan touches `lib/McsCore/src/domain/`, `ports/`, or `test/`.
- ACN risk: the `ControllerNode` commit is real new behavior with zero test coverage (no native equivalent exists), so it is classified `!` (risky, hand-verified via build-check only) — the `^`/8-LoC rule for `F` commits does not apply here since there are no matching tests, full stop.

---

### Task 1: `ControllerNode` composition root + wire `main.cpp`

**Files:**
- Create: `lib/McsCore/src/adapters/ControllerNode.h`
- Modify: `src/main.cpp`

**Interfaces consumed** (all already built, all signatures verified against the current source before writing this plan):
- `NodeConfig::factoryDefault()`, `.id()`, `.wifi()`, `.broker()`, `.turnouts()` (`lib/McsCore/src/domain/NodeConfig.h`)
- `TurnoutConfig::outputPin()`, `.feedbackPin()`, `.orientation()`, `.settleDuration()`, `.movementTimeout()` (`lib/McsCore/src/domain/TurnoutConfig.h`)
- `ConfigStore::load()` (`lib/McsCore/src/ports/ConfigStore.h`), implemented by `NvsConfigStore` (`lib/McsCore/src/adapters/NvsConfigStore.h`)
- `ArduinoClock::now()` (`lib/McsCore/src/adapters/ArduinoClock.h`)
- `EspDigitalOutput(int pin)` (`lib/McsCore/src/adapters/EspDigitalOutput.h`)
- `EspDigitalInput(int pin, bool hasInternalPullUp)` (`lib/McsCore/src/adapters/EspDigitalInput.h`)
- `WiFiLink(Clock&, Duration retryInterval)`, `.begin(const WifiCredentials&)`, `.poll()` (`lib/McsCore/src/adapters/WiFiLink.h`)
- `MqttLink(Clock&, Duration retryInterval, std::string clientId, std::string willTopic, std::string willMessage)`, `.begin(const BrokerAddress&)`, `.poll()` (`lib/McsCore/src/adapters/MqttLink.h`)
- `MqttPositionReporter(MqttLink&)` implements `PositionReporter` (`lib/McsCore/src/adapters/MqttPositionReporter.h`)
- `MqttCommandSource(MqttLink&, TurnoutCommandSink&)`, `.subscribeAll(int nodeId)` (`lib/McsCore/src/adapters/MqttCommandSource.h`)
- `FeedbackSensor(DigitalInput&, Orientation, Duration stableDuration)` (`lib/McsCore/src/domain/FeedbackSensor.h`)
- `TurnoutMotion(TurnoutPosition initialPosition, Duration movementTimeout, Duration settleDuration)` (`lib/McsCore/src/domain/TurnoutMotion.h`)
- `Turnout(TurnoutId, DigitalOutput&, Orientation, FeedbackSensor, TurnoutMotion, PositionReporter&)` — takes `sensor`/`motion` **by value** (moved in), owns them internally (`lib/McsCore/src/domain/Turnout.h`)
- `TurnoutRegistry(int nodeId, std::array<Turnout, 8> turnouts)` — takes `turnouts` **by value** (moved in), implements `TurnoutCommandSink` (`lib/McsCore/src/domain/TurnoutRegistry.h`)
- `TurnoutId(int value)`, `TurnoutPosition::closed()`, `Duration(unsigned long milliseconds)` (value objects)

**Design decisions this task makes (not specified elsewhere — call these out to the user before executing):**
1. **Feedback debounce window (20ms):** `FeedbackSensor`'s third constructor argument (`stableDuration`) has no home in `TurnoutConfig` (which only has `settleDuration`/`movementTimeout`, semantically different — mechanical settle time and fault timeout, not raw-contact debounce). This plan hardcodes `kFeedbackDebounceMs = 20` as a `ControllerNode`-private constant rather than adding a speculative `NodeConfig` field, per needs-driven building. Revisit if a real need for per-node tuning shows up.
2. **Link retry interval (5000ms):** same treatment for `WiFiLink`/`MqttLink`'s `retryInterval` — a private constant, not a config field.
3. **Initial assumed position is always `closed()`:** position is deliberately not persisted (design doc: "a remembered value is strictly worse than a measurement"). Every `TurnoutMotion` boots with `target_ = closed()`, `AtRest`. If real hardware disagrees, `TurnoutMotion::update()`'s `AtRest` branch already handles this correctly — it flags `Faulted` (published as JMRI `UNKNOWN`) rather than lying about position, and self-heals once a real command matches reality. This is existing, already-tested `TurnoutMotion` behavior, not new logic — just documenting the consequence of always seeding `closed()`.
4. **GPIO pull-up capability:** GPIO 34/35/36/39 on the ESP32-WROOM-32 are input-only and have no internal pull resistor. A private `hasInternalPullUp(int pin)` helper encodes this hardware fact for wiring `EspDigitalInput`.
5. **MQTT identity:** client id `"mcs-node-<id>"`, Last-Will-and-Testament topic `"node/<id>/status"`, message `"offline"`. `MqttLink` already supports an LWT (built in #15); this just supplies concrete values. This is *not* the full presence-announce feature (`NodePresenceReporter`, backlog #20) — just using the LWT mechanism that already exists.
6. **Scope boundary:** this task assumes a valid, already-commissioned `NodeConfig`. On a factory-default board (`id=0`, pins=`-1`), `EspDigitalOutput`/`EspDigitalInput` will be constructed against pin `-1`, which is untested and not expected to behave sensibly — that's fine, because there is no way to *reach* a real board in that state without bench commissioning (backlog #18, not yet built) writing real values first. Not this task's problem to solve.

- [ ] **Step 1: Write `lib/McsCore/src/adapters/ControllerNode.h`**

```cpp
#pragma once

#ifdef ARDUINO

#include <array>
#include <string>

#include "domain/NodeConfig.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/Duration.h"
#include "domain/FeedbackSensor.h"
#include "domain/TurnoutMotion.h"
#include "domain/Turnout.h"
#include "domain/TurnoutRegistry.h"
#include "adapters/ArduinoClock.h"
#include "adapters/EspDigitalOutput.h"
#include "adapters/EspDigitalInput.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/WiFiLink.h"
#include "adapters/MqttLink.h"
#include "adapters/MqttPositionReporter.h"
#include "adapters/MqttCommandSource.h"

class ControllerNode
{
public:
    ControllerNode()
    {
    }

    void begin()
    {
        wifiLink_.begin(config_.wifi());
        mqttLink_.begin(config_.broker());
        commandSource_.subscribeAll(config_.id().value());
    }

    void tick()
    {
        wifiLink_.poll();
        mqttLink_.poll();
        registry_.tick(clock_.now());
    }

private:
    // GPIO 34/35/36/39 are input-only on the ESP32-WROOM-32 and have no
    // internal pull resistor; every other pin does.
    static bool hasInternalPullUp(int pin)
    {
        return pin != 34 && pin != 35 && pin != 36 && pin != 39;
    }

    static TurnoutId globalTurnoutId(const NodeConfig& config, int channel)
    {
        return TurnoutId(config.id().value() * 100 + channel);
    }

    // Debounce window for raw feedback contact reads. Deliberately separate
    // from TurnoutConfig's settleDuration (mechanical settle time) and
    // movementTimeout (fault detection) - smooths electrical noise before
    // the motion state machine ever sees a position.
    static constexpr unsigned long kFeedbackDebounceMs = 20;
    static constexpr unsigned long kLinkRetryMs = 5000;

    NvsConfigStore configStore_;
    const NodeConfig config_{configStore_.load()};
    ArduinoClock clock_;

    std::array<EspDigitalOutput, 8> outputs_{
        EspDigitalOutput(config_.turnouts()[0].outputPin()),
        EspDigitalOutput(config_.turnouts()[1].outputPin()),
        EspDigitalOutput(config_.turnouts()[2].outputPin()),
        EspDigitalOutput(config_.turnouts()[3].outputPin()),
        EspDigitalOutput(config_.turnouts()[4].outputPin()),
        EspDigitalOutput(config_.turnouts()[5].outputPin()),
        EspDigitalOutput(config_.turnouts()[6].outputPin()),
        EspDigitalOutput(config_.turnouts()[7].outputPin())};

    std::array<EspDigitalInput, 8> inputs_{
        EspDigitalInput(config_.turnouts()[0].feedbackPin(), hasInternalPullUp(config_.turnouts()[0].feedbackPin())),
        EspDigitalInput(config_.turnouts()[1].feedbackPin(), hasInternalPullUp(config_.turnouts()[1].feedbackPin())),
        EspDigitalInput(config_.turnouts()[2].feedbackPin(), hasInternalPullUp(config_.turnouts()[2].feedbackPin())),
        EspDigitalInput(config_.turnouts()[3].feedbackPin(), hasInternalPullUp(config_.turnouts()[3].feedbackPin())),
        EspDigitalInput(config_.turnouts()[4].feedbackPin(), hasInternalPullUp(config_.turnouts()[4].feedbackPin())),
        EspDigitalInput(config_.turnouts()[5].feedbackPin(), hasInternalPullUp(config_.turnouts()[5].feedbackPin())),
        EspDigitalInput(config_.turnouts()[6].feedbackPin(), hasInternalPullUp(config_.turnouts()[6].feedbackPin())),
        EspDigitalInput(config_.turnouts()[7].feedbackPin(), hasInternalPullUp(config_.turnouts()[7].feedbackPin()))};

    WiFiLink wifiLink_{clock_, Duration(kLinkRetryMs)};

    MqttLink mqttLink_{
        clock_,
        Duration(kLinkRetryMs),
        "mcs-node-" + std::to_string(config_.id().value()),
        "node/" + std::to_string(config_.id().value()) + "/status",
        "offline"};

    MqttPositionReporter positionReporter_{mqttLink_};

    TurnoutRegistry registry_{
        config_.id().value(),
        std::array<Turnout, 8>{
            Turnout(globalTurnoutId(config_, 1), outputs_[0], config_.turnouts()[0].orientation(),
                    FeedbackSensor(inputs_[0], config_.turnouts()[0].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[0].movementTimeout(), config_.turnouts()[0].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 2), outputs_[1], config_.turnouts()[1].orientation(),
                    FeedbackSensor(inputs_[1], config_.turnouts()[1].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[1].movementTimeout(), config_.turnouts()[1].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 3), outputs_[2], config_.turnouts()[2].orientation(),
                    FeedbackSensor(inputs_[2], config_.turnouts()[2].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[2].movementTimeout(), config_.turnouts()[2].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 4), outputs_[3], config_.turnouts()[3].orientation(),
                    FeedbackSensor(inputs_[3], config_.turnouts()[3].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[3].movementTimeout(), config_.turnouts()[3].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 5), outputs_[4], config_.turnouts()[4].orientation(),
                    FeedbackSensor(inputs_[4], config_.turnouts()[4].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[4].movementTimeout(), config_.turnouts()[4].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 6), outputs_[5], config_.turnouts()[5].orientation(),
                    FeedbackSensor(inputs_[5], config_.turnouts()[5].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[5].movementTimeout(), config_.turnouts()[5].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 7), outputs_[6], config_.turnouts()[6].orientation(),
                    FeedbackSensor(inputs_[6], config_.turnouts()[6].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[6].movementTimeout(), config_.turnouts()[6].settleDuration()),
                    positionReporter_),
            Turnout(globalTurnoutId(config_, 8), outputs_[7], config_.turnouts()[7].orientation(),
                    FeedbackSensor(inputs_[7], config_.turnouts()[7].orientation(), Duration(kFeedbackDebounceMs)),
                    TurnoutMotion(TurnoutPosition::closed(), config_.turnouts()[7].movementTimeout(), config_.turnouts()[7].settleDuration()),
                    positionReporter_)}};

    MqttCommandSource commandSource_{mqttLink_, registry_};
};

#endif
```

Note on legality: default member initializers referencing earlier-declared
members (e.g. `outputs_`'s initializer using `config_`) are well-defined C++17
— members initialize in declaration order, and each initializer executes with
all earlier members already constructed. `TurnoutRegistry`/`Turnout`'s
by-value `sensor`/`motion`/`turnouts` parameters get mandatory copy elision
from the prvalue temporaries constructed here (C++17), so no extra copies
happen despite the reference members (`DigitalOutput&`, `PositionReporter&`)
those classes hold internally.

- [ ] **Step 2: Wire `src/main.cpp` for real**

Replace the no-op stub with the actual composition root:

```cpp
#include <Arduino.h>

#include "adapters/ControllerNode.h"

// ControllerNode is constructed here (function-local static, not file-scope)
// because its constructor performs real NVS/GPIO work via NvsConfigStore and
// the Esp*Output/Input adapters. A file-scope static would run before
// app_main() calls initArduino() (see cores/esp32/main.cpp), which is the
// only place nvs_flash_init() runs (esp32-hal-misc.c) - constructing before
// that silently drops all saved NVS config to factory defaults. setup() only
// runs after initArduino(), so this ordering is safe.
static ControllerNode* node = nullptr;

void setup()
{
    static ControllerNode instance;
    node = &instance;
    node->begin();
}

void loop()
{
    node->tick();
}
```

**Correction (post-build-check):** the plan originally showed a file-scope
`static ControllerNode node;`. A whole-branch code review caught that this
runs its constructor — and therefore `NvsConfigStore::load()` →
`Preferences::begin()` → `nvs_open()` — during C++ global static
initialization, which happens *before* `app_main()` calls `initArduino()`
(the only place `nvs_flash_init()` runs). `nvs_open()` before
`nvs_flash_init()` fails, so `Preferences::begin()` returns `false` and every
`getInt`/`getString` in `NvsConfigStore::load()` silently falls back to its
default, meaning `config_` always resolved to `NodeConfig::factoryDefault()`
even on a board with valid saved NVS data. The fix above moves construction
into a function-local static inside `setup()`, which only runs after
`initArduino()` has already run (confirmed via `loopTask()` in the same
`cores/esp32/main.cpp`) — same "exactly one `ControllerNode` for the
program's lifetime" guarantee, correct ordering. The code actually built and
committed matches this corrected version, not the original.

- [ ] **Step 3: Build-check**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — full compile and link, no errors. This is the
verification method for this task (see Global Constraints) — there is no
native equivalent to run.

If it fails, fix `ControllerNode.h` and re-run. Do not proceed to Step 4
until this is clean.

- [ ] **Step 4: Verify native suite is unaffected**

Run: `pio test -e native`
Expected: 25/25 test binaries still pass (nothing under `lib/McsCore/src/domain/`,
`ports/`, or `test/` changed in this task).

- [ ] **Step 5: Commit**

Present the commit plan to the user via the `/arlo-commits` process before
running any git commands:

```
! F Add ControllerNode composition root, wire main.cpp
```

Both files (`lib/McsCore/src/adapters/ControllerNode.h`, `src/main.cpp`) are
one piece of work — the class only means anything once `main.cpp` actually
instantiates it — so they land in a single commit.

```bash
git add lib/McsCore/src/adapters/ControllerNode.h src/main.cpp
git commit -m "$(cat <<'EOF'
! F Add ControllerNode composition root, wire main.cpp

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01WcPGxByk7AdHMiFUqv5HbA
EOF
)"
```

---

### Task 2: Mark backlog #16 complete in status docs

**Files:**
- Modify: `docs/task-status.md`

- [ ] **Step 1: Move backlog #16 into the Completed table**

Add a row to the Completed table (after the ESP32 adapters row), citing
Task 1's actual commit hash once known, and add these two notes to "Known
scaffolding debt":

- `ControllerNode`'s debounce/retry durations (`kFeedbackDebounceMs = 20`,
  `kLinkRetryMs = 5000`) are private literals, not `NodeConfig` fields —
  revisit only if a real need for per-node tuning shows up.
- `ControllerNode` assumes an already-commissioned `NodeConfig`; on a
  factory-default board it constructs adapters against pin `-1`, which is
  untested — not reachable without bench commissioning (backlog #18)
  writing real values first.

Remove backlog #16's row from the Backlog table and its "Task details"
section; update backlog #18's "Blocked by" if it referenced #16 (it
doesn't — #18 is independently unblocked already).

- [ ] **Step 2: Commit**

```
. d Mark ControllerNode (#16) complete in status docs
```

```bash
git add docs/task-status.md
git commit -m "$(cat <<'EOF'
. d Mark ControllerNode (#16) complete in status docs

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01WcPGxByk7AdHMiFUqv5HbA
EOF
)"
```

---

## After this plan

The firmware is flashable: `pio run -e esp32dev --target upload` followed by
`pio device monitor` becomes meaningful for the first time. Bench serial
commissioning (backlog #18) is the natural next step — right now the only
way to get a real `NodeConfig` onto a board is a manual `NvsConfigStore::save()`
call from test code; there's no in-field way to set `id`/`wifi`/`broker`/pin
config without it.
