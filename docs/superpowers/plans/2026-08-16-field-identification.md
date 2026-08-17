# Field Identification & Duplicate Node ID Detection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the domain/adapter classes for backlog #20 (Field identification + duplicate node ID detection) so someone standing under the layout can confirm a node's id via LED blink-out, and so nodes can detect and refuse to fully start when another node already claims the same id.

**Architecture:** Hexagonal, same discipline as the rest of this repo (see `CLAUDE.md`). New port: `IdentifyRequestTrigger`, `NodePresenceReporter`. New pure domain classes: `BlinkOutIdentifier`, `NodeIdCollisionGuard`. New adapters: `ButtonIdentifyRequestTrigger`, `MqttNodePresenceReporter`.

**Tech Stack:** PlatformIO, Catch2 3.7.1 (native tests), Arduino framework for `esp32dev` (`MqttNodePresenceReporter` reuses the existing `MqttLink`/`PubSubClient` dependency — no new `lib_deps` needed).

## Global Constraints

- Domain/application code must compile and run under the `native` PlatformIO environment without `Arduino.h`. Only `MqttNodePresenceReporter` may include Arduino/ESP32/PubSubClient headers, and only inside `#ifdef ARDUINO`.
- No `delay()` anywhere in domain/application code. Non-blocking `poll(Instant now)` methods only (mirrors `FeedbackSensor::sample(Instant now)`).
- ACN notation for every commit message (`references/acn-notation.md` under the `arlo-commits` skill has the full spec). Never `--amend`, never `--no-verify`.
- **Deliberate architectural deviation:** `ButtonIdentifyRequestTrigger` is classified "Adapter" in `docs/software-class-list.md`, but — like `SerialCommissioningAdapter` (backlog #18) and `ButtonSetupModeTrigger` (backlog #19, developed concurrently with this plan on a separate branch — do not assume its files exist in this workspace) — it depends only on the pure `DigitalInput` port and domain value objects, never on `Arduino.h` directly. Give it **no** `#ifdef ARDUINO` guard, and give it full native TDD coverage. This is intentional — do not "fix" it to match `MqttNodePresenceReporter`'s guard style, and do not flag it as an inconsistency in review.
- This plan does **not** wire any of these classes into `ControllerNode`/`src/main.cpp`. That mirrors the established split from backlog #15→#16 and #17→#18: build and verify the classes now, wire them into the composition root in a later, separate task once the composition root actually needs boot-time id-collision checking and runtime identify-blink handling. Do not add that wiring in this plan.
- `MqttNodePresenceReporter` has no native equivalent (real MQTT broker). Verify it with the **build-check cycle** established in backlog #15/#16/#18: temporarily wire the class into `src/main.cpp`, run `pio run -e esp32dev`, then revert `main.cpp` to its exact original content and rebuild to confirm the revert is clean. Confirm via `git diff src/main.cpp` that it shows zero output right after the revert, before committing.
- **This plan runs concurrently with a separate wireless-commissioning plan (backlog #19) on another branch.** They touch disjoint files except `docs/task-status.md`, which both plans' final task updates — expect a merge conflict there when the two branches are combined; that is normal and will be resolved by the human/coordinator combining both additions, not something to avoid by skipping the docs update.

---

### Task 1: `BlinkOutIdentifier` domain class

**Files:**
- Create: `lib/McsCore/src/domain/BlinkOutIdentifier.h`
- Test: `test/test_blink_out_identifier/test_main.cpp`

**Interfaces:**
- Consumes: `NodeId` (existing, `lib/McsCore/src/domain/NodeId.h` — `int value() const`), `Duration`/`Level` (existing).
- Produces: `BlinkOutIdentifier` — not consumed by any other task in this plan (composition-root wiring is deferred, per Global Constraints).

Given a `NodeId` and blink/pause timing, produces the on/off `Level` for the status LED at any elapsed time since the blink-out sequence started: blink `onDuration` on, then `offDuration` off, repeated `id.value()` times, then `pauseDuration` off, then the cycle repeats. Pure and stateless — a single method mapping elapsed `Duration` to `Level`, no internal mutable state, no `Clock` dependency (the caller computes elapsed time using whatever clock it has). This satisfies the design doc's "Given a NodeId and a Clock, produces the on/off Level sequence... testable with ManualClock" — this repo's equivalent of "ManualClock" is the existing `FakeClock`; a plain `Duration` parameter is the simplest pure interface and needs no `Clock`/`FakeClock` include here at all.

**Precondition (not defensively checked, matches CLAUDE.md's "trust internal code, validate only at boundaries"):** assumes `id.value() >= 1` and `pauseDuration.milliseconds() > 0`. A `NodeId` reaching this class is only ever a real, commissioned id (1-16) — `NodeConfig::factoryDefault()`'s invalid id-0 sentinel is rejected by `NodeConfig::validate()` long before a node would run this class.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_blink_out_identifier/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/BlinkOutIdentifier.h"
#include "domain/NodeId.h"

TEST_CASE("BlinkOutIdentifier blinks on immediately at elapsed zero")
{
    BlinkOutIdentifier identifier(NodeId(3), Duration(200), Duration(200), Duration(1000));
    REQUIRE(identifier.levelAt(Duration(0)) == Level::High);
}

TEST_CASE("BlinkOutIdentifier turns off after onDuration within one blink")
{
    BlinkOutIdentifier identifier(NodeId(3), Duration(200), Duration(200), Duration(1000));
    REQUIRE(identifier.levelAt(Duration(250)) == Level::Low);
}

TEST_CASE("BlinkOutIdentifier produces N on/off blinks for id N")
{
    // id=2: blink period 400ms (200 on + 200 off). Blink 1: [0,200)=On [200,400)=Off.
    // Blink 2: [400,600)=On [600,800)=Off. Then pause.
    BlinkOutIdentifier identifier(NodeId(2), Duration(200), Duration(200), Duration(1000));

    REQUIRE(identifier.levelAt(Duration(0)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(199)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(200)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(399)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(400)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(599)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(600)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(799)) == Level::Low);
}

TEST_CASE("BlinkOutIdentifier is off during the pause phase after all blinks")
{
    // id=2: 2 blinks * 400ms = 800ms of blinking, then 1000ms pause -> [800, 1800) = Low.
    BlinkOutIdentifier identifier(NodeId(2), Duration(200), Duration(200), Duration(1000));

    REQUIRE(identifier.levelAt(Duration(800)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(1500)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(1799)) == Level::Low);
}

TEST_CASE("BlinkOutIdentifier repeats the full cycle after the pause")
{
    // id=2: full cycle = 800ms blinking + 1000ms pause = 1800ms.
    BlinkOutIdentifier identifier(NodeId(2), Duration(200), Duration(200), Duration(1000));

    REQUIRE(identifier.levelAt(Duration(1800)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(1800 + 199)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(1800 + 200)) == Level::Low);
}

TEST_CASE("BlinkOutIdentifier scales the blink count with a different id")
{
    // id=1: blink period 200ms (100 on + 100 off).
    BlinkOutIdentifier identifier(NodeId(1), Duration(100), Duration(100), Duration(500));

    REQUIRE(identifier.levelAt(Duration(0)) == Level::High);
    REQUIRE(identifier.levelAt(Duration(100)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(199)) == Level::Low);
    REQUIRE(identifier.levelAt(Duration(200)) == Level::Low); // pause phase starts (1 blink * 200ms)
    REQUIRE(identifier.levelAt(Duration(699)) == Level::Low); // still pause (200 + 500 = 700ms cycle)
    REQUIRE(identifier.levelAt(Duration(700)) == Level::High); // cycle repeats
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_blink_out_identifier`
Expected: FAIL to compile — `domain/BlinkOutIdentifier.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/McsCore/src/domain/BlinkOutIdentifier.h
#pragma once

#include "domain/NodeId.h"
#include "domain/Duration.h"
#include "domain/Level.h"

class BlinkOutIdentifier
{
public:
    BlinkOutIdentifier(NodeId id, Duration onDuration, Duration offDuration, Duration pauseDuration)
        : id_(id), onDuration_(onDuration), offDuration_(offDuration), pauseDuration_(pauseDuration)
    {
    }

    Level levelAt(Duration elapsed) const
    {
        unsigned long blinkPeriodMs = onDuration_.milliseconds() + offDuration_.milliseconds();
        unsigned long blinkingPhaseMs = blinkPeriodMs * static_cast<unsigned long>(id_.value());
        unsigned long cycleMs = blinkingPhaseMs + pauseDuration_.milliseconds();

        unsigned long t = elapsed.milliseconds() % cycleMs;

        if (t >= blinkingPhaseMs)
        {
            return Level::Low;
        }

        unsigned long withinBlink = t % blinkPeriodMs;
        return withinBlink < onDuration_.milliseconds() ? Level::High : Level::Low;
    }

private:
    NodeId id_;
    Duration onDuration_;
    Duration offDuration_;
    Duration pauseDuration_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_blink_out_identifier`
Expected: PASS, 6 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/domain/BlinkOutIdentifier.h test/test_blink_out_identifier/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add BlinkOutIdentifier domain class

EOF
)"
```

---

### Task 2: `IdentifyRequestTrigger` port + fake

**Files:**
- Create: `lib/McsCore/src/ports/IdentifyRequestTrigger.h`
- Create: `test/support/FakeIdentifyRequestTrigger.h`
- Test: `test/test_identify_request_trigger_fake/test_main.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `IdentifyRequestTrigger` (consumed by Task 3's `ButtonIdentifyRequestTrigger`).

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_identify_request_trigger_fake/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeIdentifyRequestTrigger.h"

TEST_CASE("FakeIdentifyRequestTrigger defaults to not requested")
{
    FakeIdentifyRequestTrigger trigger;
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("FakeIdentifyRequestTrigger reports whatever was set")
{
    FakeIdentifyRequestTrigger trigger;
    trigger.setRequested(true);
    REQUIRE(trigger.requested());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_identify_request_trigger_fake`
Expected: FAIL to compile — neither header exists yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/McsCore/src/ports/IdentifyRequestTrigger.h
#pragma once

class IdentifyRequestTrigger
{
public:
    virtual ~IdentifyRequestTrigger() = default;
    virtual bool requested() const = 0;
};
```

```cpp
// test/support/FakeIdentifyRequestTrigger.h
#pragma once

#include "ports/IdentifyRequestTrigger.h"

class FakeIdentifyRequestTrigger : public IdentifyRequestTrigger
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

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_identify_request_trigger_fake`
Expected: PASS, 2 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/ports/IdentifyRequestTrigger.h test/support/FakeIdentifyRequestTrigger.h \
        test/test_identify_request_trigger_fake/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add IdentifyRequestTrigger port

EOF
)"
```

---

### Task 3: `ButtonIdentifyRequestTrigger` (native-tested, deliberate deviation)

**Files:**
- Create: `lib/McsCore/src/adapters/ButtonIdentifyRequestTrigger.h`
- Test: `test/test_button_identify_request_trigger/test_main.cpp`

**Interfaces:**
- Consumes: `IdentifyRequestTrigger` (Task 2, base class), `DigitalInput` port (existing — `Level read()`), `FakeDigitalInput` (existing — `enqueue(Level)`), `Duration`/`Instant`/`Level` (existing).
- Produces: `ButtonIdentifyRequestTrigger` — not consumed by any other task in this plan (composition-root wiring is deferred, per Global Constraints).

**This is the deliberate architectural deviation called out in Global Constraints.** Detects a short press-and-release of the BOOT pin (active-low: pressed = `Level::Low`) via a non-blocking `poll(Instant now)` method: tracks when the pin goes low (press start), and on the tick it goes back high, checks whether the press duration was within `[minPressDuration, maxPressDuration]`. `requested()` is edge-triggered — true only on the `poll()` call where a qualifying short press was just released, false on every other call (mirrors the design doc's "was BOOT short-pressed *this tick*").

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_button_identify_request_trigger/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/ButtonIdentifyRequestTrigger.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("ButtonIdentifyRequestTrigger is not requested while the button is still held")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonIdentifyRequestTrigger fires on the tick a qualifying short press is released")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(150));

    REQUIRE(trigger.requested());
}

TEST_CASE("ButtonIdentifyRequestTrigger is edge-triggered - true for one tick only")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    bootPin.enqueue(Level::High);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(150));
    REQUIRE(trigger.requested());

    trigger.poll(Instant(300));
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonIdentifyRequestTrigger ignores a press shorter than the debounce minimum")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(10));

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonIdentifyRequestTrigger ignores a press held longer than the max (that's a setup-mode hold, not a short press)")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(2500));

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("ButtonIdentifyRequestTrigger can fire again on a second qualifying short press")
{
    FakeDigitalInput bootPin;
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    bootPin.enqueue(Level::Low);
    bootPin.enqueue(Level::High);
    ButtonIdentifyRequestTrigger trigger(bootPin, Duration(30), Duration(2000));

    trigger.poll(Instant(0));
    trigger.poll(Instant(150));
    REQUIRE(trigger.requested());

    trigger.poll(Instant(1000));
    trigger.poll(Instant(1150));
    REQUIRE(trigger.requested());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_button_identify_request_trigger`
Expected: FAIL to compile — `adapters/ButtonIdentifyRequestTrigger.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/McsCore/src/adapters/ButtonIdentifyRequestTrigger.h
#pragma once

#include "ports/IdentifyRequestTrigger.h"
#include "ports/DigitalInput.h"
#include "domain/Level.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

class ButtonIdentifyRequestTrigger : public IdentifyRequestTrigger
{
public:
    ButtonIdentifyRequestTrigger(DigitalInput& bootPin, Duration minPressDuration, Duration maxPressDuration)
        : bootPin_(bootPin), minPressDuration_(minPressDuration), maxPressDuration_(maxPressDuration)
    {
    }

    // Call repeatedly with the current time. Non-blocking - no delay().
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
            if (heldFor >= minPressDuration_ && heldFor <= maxPressDuration_)
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
    Duration minPressDuration_;
    Duration maxPressDuration_;
    bool pressed_ = false;
    Instant pressStart_ = Instant(0);
    bool requestedThisTick_ = false;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_button_identify_request_trigger`
Expected: PASS, 6 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/adapters/ButtonIdentifyRequestTrigger.h test/test_button_identify_request_trigger/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add ButtonIdentifyRequestTrigger (native-tested, no ARDUINO guard)

Deliberate deviation from the adapter/ARDUINO-guard convention, same
rationale as SerialCommissioningAdapter (backlog #18): depends only on
the pure DigitalInput port and domain value objects, so it earns full
native TDD coverage instead of build-check-only verification.

EOF
)"
```

---

### Task 4: `NodePresenceReporter` port + fake

**Files:**
- Create: `lib/McsCore/src/ports/NodePresenceReporter.h`
- Create: `test/support/FakeNodePresenceReporter.h`
- Test: `test/test_node_presence_reporter_fake/test_main.cpp`

**Interfaces:**
- Consumes: `NodeId` (existing).
- Produces: `NodePresenceReporter` (consumed by Task 6's `MqttNodePresenceReporter`).

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_node_presence_reporter_fake/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeNodePresenceReporter.h"
#include "domain/NodeId.h"

TEST_CASE("FakeNodePresenceReporter starts with no announcements")
{
    FakeNodePresenceReporter reporter;
    REQUIRE(reporter.announced().empty());
}

TEST_CASE("FakeNodePresenceReporter records announcements in order")
{
    FakeNodePresenceReporter reporter;

    reporter.announce(NodeId(3));
    reporter.announce(NodeId(5));

    REQUIRE(reporter.announced().size() == 2);
    REQUIRE(reporter.announced()[0] == NodeId(3));
    REQUIRE(reporter.announced()[1] == NodeId(5));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_node_presence_reporter_fake`
Expected: FAIL to compile — neither header exists yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/McsCore/src/ports/NodePresenceReporter.h
#pragma once

#include "domain/NodeId.h"

class NodePresenceReporter
{
public:
    virtual ~NodePresenceReporter() = default;
    virtual void announce(NodeId id) = 0;
};
```

```cpp
// test/support/FakeNodePresenceReporter.h
#pragma once

#include <vector>

#include "ports/NodePresenceReporter.h"

class FakeNodePresenceReporter : public NodePresenceReporter
{
public:
    void announce(NodeId id) override
    {
        announced_.push_back(id);
    }

    const std::vector<NodeId>& announced() const
    {
        return announced_;
    }

private:
    std::vector<NodeId> announced_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_node_presence_reporter_fake`
Expected: PASS, 2 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/ports/NodePresenceReporter.h test/support/FakeNodePresenceReporter.h \
        test/test_node_presence_reporter_fake/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add NodePresenceReporter port

EOF
)"
```

---

### Task 5: `NodeIdCollisionGuard` domain class

**Files:**
- Create: `lib/McsCore/src/domain/NodeIdCollisionGuard.h`
- Test: `test/test_node_id_collision_guard/test_main.cpp`

**Interfaces:**
- Consumes: `NodeId` (existing).
- Produces: `NodeIdCollisionGuard`, `PresenceVerdict` — not consumed by any other task in this plan (composition-root wiring is deferred, per Global Constraints).

Per the design doc, a node subscribes to its own presence topic *before* announcing itself, so it can see whether another node already claims its id. `NodeIdCollisionGuard::evaluate` is the pure decision at the heart of that check: given the id a presence message was observed on, and whether this node has already announced its own presence yet, decide:

- The observed id doesn't match this node's own id → **Unrelated** (a different node's presence message — expected background noise from a broker-wide `node/+/status` subscription, not something to react to).
- The observed id matches this node's own id, and this node **hasn't** announced itself yet → **Collision** (nobody but a *different* boot session could have published this — refuse to fully come online).
- The observed id matches this node's own id, and this node **has already** announced itself → **Self** (this is this node's own retained publish echoing back over its own subscription — ignore it).

This keeps the guard pure and testable with plain values (no broker, no timing) — the "have I announced yet" ordering is a simple boolean the caller (a later, out-of-scope composition-root task) tracks and passes in.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_node_id_collision_guard/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/NodeIdCollisionGuard.h"
#include "domain/NodeId.h"

TEST_CASE("NodeIdCollisionGuard treats a message about a different id as unrelated")
{
    NodeIdCollisionGuard guard(NodeId(3));

    REQUIRE(guard.evaluate(NodeId(7), false) == PresenceVerdict::Unrelated);
    REQUIRE(guard.evaluate(NodeId(7), true) == PresenceVerdict::Unrelated);
}

TEST_CASE("NodeIdCollisionGuard treats a same-id message before this node announced as a collision")
{
    NodeIdCollisionGuard guard(NodeId(3));

    REQUIRE(guard.evaluate(NodeId(3), false) == PresenceVerdict::Collision);
}

TEST_CASE("NodeIdCollisionGuard treats a same-id message after this node announced as self")
{
    NodeIdCollisionGuard guard(NodeId(3));

    REQUIRE(guard.evaluate(NodeId(3), true) == PresenceVerdict::Self);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_node_id_collision_guard`
Expected: FAIL to compile — `domain/NodeIdCollisionGuard.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/McsCore/src/domain/NodeIdCollisionGuard.h
#pragma once

#include "domain/NodeId.h"

enum class PresenceVerdict
{
    Self,
    Collision,
    Unrelated
};

class NodeIdCollisionGuard
{
public:
    explicit NodeIdCollisionGuard(NodeId selfId) : selfId_(selfId)
    {
    }

    PresenceVerdict evaluate(NodeId observedId, bool hasAnnouncedSelf) const
    {
        if (observedId != selfId_)
        {
            return PresenceVerdict::Unrelated;
        }

        return hasAnnouncedSelf ? PresenceVerdict::Self : PresenceVerdict::Collision;
    }

private:
    NodeId selfId_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_node_id_collision_guard`
Expected: PASS, 3 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/domain/NodeIdCollisionGuard.h test/test_node_id_collision_guard/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add NodeIdCollisionGuard domain class

EOF
)"
```

---

### Task 6: `MqttNodePresenceReporter` (build-check only, Arduino)

**Files:**
- Create: `lib/McsCore/src/adapters/MqttNodePresenceReporter.h`
- Modify (temporarily, then revert): `src/main.cpp`

**Interfaces:**
- Consumes: `NodePresenceReporter` (Task 4), `MqttLink` (existing, `lib/McsCore/src/adapters/MqttLink.h` — `PubSubClient& raw()`), `NodeId` (existing).
- Produces: nothing consumed by a later task in this plan — a leaf adapter, verified only by build-check (no native equivalent exists for a real MQTT broker).

`#ifdef ARDUINO`-guarded, matching every other hardware adapter (`MqttPositionReporter` is the closest sibling — same shape: wraps `MqttLink::raw()`). Publishes a **retained** `"online"` message to `node/<id>/status`.

- [ ] **Step 1: Write the implementation**

```cpp
// lib/McsCore/src/adapters/MqttNodePresenceReporter.h
#pragma once

#ifdef ARDUINO

#include <string>

#include "adapters/MqttLink.h"
#include "ports/NodePresenceReporter.h"
#include "domain/NodeId.h"

class MqttNodePresenceReporter : public NodePresenceReporter
{
public:
    explicit MqttNodePresenceReporter(MqttLink& link) : link_(link)
    {
    }

    void announce(NodeId id) override
    {
        std::string topic = "node/" + std::to_string(id.value()) + "/status";
        link_.raw().publish(topic.c_str(), "online", true);
    }

private:
    MqttLink& link_;
};

#endif
```

- [ ] **Step 2: Build-check it**

`src/main.cpp` is currently just a thin composition root (`#include <Arduino.h>` + `adapters/ControllerNode.h`, a `setup()` that constructs one function-local-static `ControllerNode` and calls `begin()`, and a `loop()` that calls `tick()`) — it does **not** already have a `MqttLink`/`NodeId` in scope to reuse (those are private members inside `ControllerNode`). Build a minimal, self-contained instance instead; do not reach into `ControllerNode`'s internals.

Record the file's original content first with `git diff src/main.cpp` (expect no output) so you can confirm an exact revert later. Then temporarily add these includes near the top:

```cpp
#include "adapters/MqttNodePresenceReporter.h"
#include "adapters/MqttLink.h"
#include "adapters/ArduinoClock.h"
#include "domain/NodeId.h"
#include "domain/Duration.h"
```

and, inside `setup()`, before `node->begin();`:

```cpp
static ArduinoClock clockForBuildCheck;
static MqttLink mqttLinkForBuildCheck(clockForBuildCheck, Duration(5000), "build-check", "build-check/status", "offline");
static MqttNodePresenceReporter presenceReporterForBuildCheck(mqttLinkForBuildCheck);
presenceReporterForBuildCheck.announce(NodeId(1));
```

This only proves the header compiles and links against real ESP32/PubSubClient headers — it is not wired into the real boot sequence and must be fully removed in Step 3.

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 3: Revert `src/main.cpp`**

Revert every temporary edit from Step 2 so `src/main.cpp` is byte-identical to what it was before this task. Verify:

```bash
git diff src/main.cpp
```

Expected: no output. Then rebuild once more to confirm the revert didn't break anything:

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add lib/McsCore/src/adapters/MqttNodePresenceReporter.h
git commit -m "$(cat <<'EOF'
! F Add MqttNodePresenceReporter adapter

Build-check verified only (pio run -e esp32dev, src/main.cpp reverted
after) - no native equivalent exists for a real MQTT broker.

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

Add a row documenting this feature, citing the real commit hashes from Tasks 1-6 above (look them up with `git log --oneline` — do not guess). Follow the existing row style exactly (see the `Bench serial commissioning` row for the level of detail expected).

- [ ] **Step 2: Update the Backlog table**

Remove backlog item `#20` from the Backlog table (it's now done). **A separate, concurrent plan (backlog #19, wireless commissioning) may be editing this same table on another branch at the same time** — if you can see evidence of its changes already merged (e.g. `#19` already absent from the table), do not re-add it or assume anything about its content; just make your own #20 edit relative to whatever the table currently contains. If a merge conflict arises later when this branch is merged with that one, that is expected and should be resolved by combining both branches' additions, not by picking one side.

- [ ] **Step 3: Update the native test count**

Update the running native test binary count and the list of newly added binaries (this plan adds `test_blink_out_identifier`, `test_identify_request_trigger_fake`, `test_button_identify_request_trigger`, `test_node_presence_reporter_fake`, `test_node_id_collision_guard` — 5 new binaries; `MqttNodePresenceReporter` is build-check-only, not a native binary).

- [ ] **Step 4: Add a scaffolding-debt note**

In "Known scaffolding debt", add a bullet noting that `ButtonIdentifyRequestTrigger`, `BlinkOutIdentifier`, `NodeIdCollisionGuard`, and `MqttNodePresenceReporter` are not yet wired into `ControllerNode`/`src/main.cpp` — no boot-time id-collision-checking or runtime identify-blink-handling logic exists yet in the composition root (same shape as the existing bench-serial-commissioning bullet).

- [ ] **Step 5: Commit**

```bash
git add docs/task-status.md
git commit -m "$(cat <<'EOF'
. d Mark Field identification complete in task-status.md

EOF
)"
```
