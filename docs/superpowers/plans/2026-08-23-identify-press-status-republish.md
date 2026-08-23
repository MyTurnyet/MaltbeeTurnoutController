# Identify Short-Press: Republish All Turnout Status Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the existing identify short-press (BOOT, 50–1500ms, `BootMode::Normal`) also republish every turnout's current MQTT status, in addition to the existing id-blink.

**Architecture:** Add a force-report path that bypasses `Turnout`'s existing on-change guard: `Turnout::reportCurrent()` reports unconditionally and updates its change-tracking state; `TurnoutRegistry::reportAll()` calls it on all 8 turnouts in id order; `ControllerNode::republishAllStatus()` forwards to the registry; `src/main.cpp` calls it from the same site that already handles `identifyTrigger->requested()`.

**Tech Stack:** C++17, PlatformIO, Catch2 (native host tests), ESP32-WROOM-32/Arduino (hardware build only).

## Global Constraints

- Domain code (`lib/McsCore/src/domain/`) must compile and run under the `native` PlatformIO environment without `Arduino.h`.
- No `delay()`/blocking calls in domain or application code.
- TDD: write a failing native test first, implement the minimum to pass.
- No new ports — reuse `PositionReporter`/`MqttPositionReporter` as-is.
- Adapter/`main.cpp` changes (Arduino-only) are verified via `pio run -e esp32dev` build-check only — there is no native equivalent for `ARDUINO`-guarded or Arduino-linked code.
- Follow existing file/test conventions exactly (see Task 1/2 for the established patterns in this repo).

---

### Task 1: `Turnout::reportCurrent()`

**Files:**
- Modify: `lib/McsCore/src/domain/Turnout.h`
- Test: `test/test_turnout/test_main.cpp`

**Interfaces:**
- Consumes: existing `Turnout` constructor (`TurnoutId id, DigitalOutput& output, Orientation orientation, FeedbackSensor sensor, TurnoutMotion motion, PositionReporter& reporter`), existing private members `id_`, `motion_`, `reporter_`, `lastReportedState_` (declared in `lib/McsCore/src/domain/Turnout.h:59-65`).
- Produces: `void reportCurrent()` — new public method on `Turnout`. Later tasks (`TurnoutRegistry::reportAll()`) call this by name.

- [ ] **Step 1: Write the failing tests**

Append to `test/test_turnout/test_main.cpp` (after the existing last `TEST_CASE`, following the exact same setup pattern used by every test in that file):

```cpp
TEST_CASE("reportCurrent republishes the current state even when it already matches the last report")
{
    FakeDigitalOutput output;
    FakeDigitalInput input;
    Orientation orientation = Orientation::normal();
    FeedbackSensor sensor(input, orientation, Duration(50));
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    FakePositionReporter reporter;
    Turnout turnout(TurnoutId(1), output, orientation, std::move(sensor), std::move(motion), reporter);
    turnout.tick(Instant(0));

    turnout.reportCurrent();

    REQUIRE(reporter.reports().size() == 2);
    REQUIRE(reporter.reports()[1].id == TurnoutId(1));
    REQUIRE(reporter.reports()[1].state == TurnoutState::Closed);
}

TEST_CASE("reportCurrent before any tick means the next tick does not duplicate the report")
{
    FakeDigitalOutput output;
    FakeDigitalInput input;
    Orientation orientation = Orientation::normal();
    FeedbackSensor sensor(input, orientation, Duration(50));
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    FakePositionReporter reporter;
    Turnout turnout(TurnoutId(1), output, orientation, std::move(sensor), std::move(motion), reporter);

    turnout.reportCurrent();
    turnout.tick(Instant(0));

    REQUIRE(reporter.reports().size() == 1);
    REQUIRE(reporter.reports()[0].state == TurnoutState::Closed);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_turnout`
Expected: compile error — `class Turnout` has no member `reportCurrent`.

- [ ] **Step 3: Implement `reportCurrent()`**

In `lib/McsCore/src/domain/Turnout.h`, add the new method right after the existing `tick(Instant now)` method (after the closing brace at line 51), before `TurnoutId id() const`:

```cpp
    void reportCurrent()
    {
        TurnoutState current = motion_.state();
        reporter_.report(id_, current);
        lastReportedState_ = current;
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_turnout`
Expected: PASS, all `TEST_CASE`s in the file green.

- [ ] **Step 5: Commit**

Use the `arlo-commits` skill (per this repo's `CLAUDE.md`) rather than a hand-written message. Expected classification: `^ F Add Turnout::reportCurrent for forced status republish` (matching test added, diff well under 8 LoC of production code).

---

### Task 2: `TurnoutRegistry::reportAll()`

**Files:**
- Modify: `lib/McsCore/src/domain/TurnoutRegistry.h`
- Test: `test/test_turnout_registry/test_main.cpp`

**Interfaces:**
- Consumes: `Turnout::reportCurrent()` (Task 1), existing private members `turnouts_` (`std::array<Turnout, TurnoutsPerNode>`, declared in `lib/McsCore/src/domain/TurnoutRegistry.h:61`).
- Produces: `void reportAll()` — new public method on `TurnoutRegistry`. Later tasks (`ControllerNode::republishAllStatus()`) call this by name.

- [ ] **Step 1: Write the failing tests**

Append to `test/test_turnout_registry/test_main.cpp` (after the existing last `TEST_CASE`, reusing the file's local `makeTurnout` helper and 8-turnout setup pattern):

```cpp
TEST_CASE("reportAll reports every turnout's current state, in id order")
{
    std::array<FakeDigitalOutput, 8> outputs;
    std::array<FakeDigitalInput, 8> inputs;
    FakePositionReporter reporter;
    std::array<Turnout, 8> turnouts{
        makeTurnout(101, outputs[0], inputs[0], reporter),
        makeTurnout(102, outputs[1], inputs[1], reporter),
        makeTurnout(103, outputs[2], inputs[2], reporter),
        makeTurnout(104, outputs[3], inputs[3], reporter),
        makeTurnout(105, outputs[4], inputs[4], reporter),
        makeTurnout(106, outputs[5], inputs[5], reporter),
        makeTurnout(107, outputs[6], inputs[6], reporter),
        makeTurnout(108, outputs[7], inputs[7], reporter)
    };
    TurnoutRegistry registry(1, std::move(turnouts));

    registry.reportAll();

    REQUIRE(reporter.reports().size() == 8);
    REQUIRE(reporter.reports()[0].id == TurnoutId(101));
    REQUIRE(reporter.reports()[0].state == TurnoutState::Closed);
    REQUIRE(reporter.reports()[7].id == TurnoutId(108));
    REQUIRE(reporter.reports()[7].state == TurnoutState::Closed);
}

TEST_CASE("reportAll republishes every turnout even when nothing has changed since the last report")
{
    std::array<FakeDigitalOutput, 8> outputs;
    std::array<FakeDigitalInput, 8> inputs;
    FakePositionReporter reporter;
    std::array<Turnout, 8> turnouts{
        makeTurnout(101, outputs[0], inputs[0], reporter),
        makeTurnout(102, outputs[1], inputs[1], reporter),
        makeTurnout(103, outputs[2], inputs[2], reporter),
        makeTurnout(104, outputs[3], inputs[3], reporter),
        makeTurnout(105, outputs[4], inputs[4], reporter),
        makeTurnout(106, outputs[5], inputs[5], reporter),
        makeTurnout(107, outputs[6], inputs[6], reporter),
        makeTurnout(108, outputs[7], inputs[7], reporter)
    };
    TurnoutRegistry registry(1, std::move(turnouts));
    registry.tick(Instant(0));

    registry.reportAll();

    REQUIRE(reporter.reports().size() == 16);
    REQUIRE(reporter.reports()[8].id == TurnoutId(101));
    REQUIRE(reporter.reports()[15].id == TurnoutId(108));
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_turnout_registry`
Expected: compile error — `class TurnoutRegistry` has no member `reportAll`.

- [ ] **Step 3: Implement `reportAll()`**

In `lib/McsCore/src/domain/TurnoutRegistry.h`, add the new method right after the existing `tick(Instant now)` method (after the closing brace at line 57), before the `private:` section:

```cpp
    void reportAll()
    {
        for (auto& turnout : turnouts_)
        {
            turnout.reportCurrent();
        }
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_turnout_registry`
Expected: PASS, all `TEST_CASE`s in the file green.

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: all binaries pass (confirms no regression in `test_turnout` from Task 1 or elsewhere).

- [ ] **Step 6: Commit**

Use the `arlo-commits` skill. Expected classification: `^ F Add TurnoutRegistry::reportAll for forced status republish` (matching tests added, diff well under 8 LoC of production code).

---

### Task 3: Wire the identify short-press to trigger a status republish

**Files:**
- Modify: `lib/McsCore/src/adapters/ControllerNode.h:71-86` (add method between `tick()` and `blocked()`)
- Modify: `src/main.cpp:190-196`

**Interfaces:**
- Consumes: `TurnoutRegistry::reportAll()` (Task 2), existing `ControllerNode` private member `registry_` (declared in `lib/McsCore/src/adapters/ControllerNode.h:144`), existing `src/main.cpp` globals `node` (`ControllerNode*`) and `identifyTrigger` (`ButtonIdentifyRequestTrigger*`).
- Produces: `void ControllerNode::republishAllStatus()` — public method. No further consumers in this plan; this is the outermost wiring.

This task is Arduino-only (`ControllerNode.h` is `#ifdef ARDUINO`-guarded; `main.cpp` links against Arduino). There is no native test for it — verify with a build check, matching this repo's existing convention for composition-root wiring (e.g. `docs/task-status.md`'s note on the original identify-blink wiring).

- [ ] **Step 1: Add `ControllerNode::republishAllStatus()`**

In `lib/McsCore/src/adapters/ControllerNode.h`, add the new public method right after the existing `tick()` method (after its closing brace, currently line 76), before the `blocked()` method:

```cpp
    // Forces every turnout to republish its current status over MQTT,
    // regardless of whether it has changed since the last publish. Used by
    // the identify short-press so JMRI (or a human checking mosquitto_sub)
    // can get a full resync on demand.
    void republishAllStatus()
    {
        registry_.reportAll();
    }
```

- [ ] **Step 2: Call it from the identify short-press handler in `src/main.cpp`**

In `src/main.cpp`, the existing block (currently lines 190–195) is:

```cpp
            identifyTrigger->poll(now);
            if (identifyTrigger->requested())
            {
                identifyDeadline.arm(now, Duration(kIdentifyActiveMs));
                identifyStart = now;
            }
```

Change it to:

```cpp
            identifyTrigger->poll(now);
            if (identifyTrigger->requested())
            {
                identifyDeadline.arm(now, Duration(kIdentifyActiveMs));
                identifyStart = now;
                node->republishAllStatus();
            }
```

- [ ] **Step 3: Build-check verify**

Run: `pio run -e esp32dev`
Expected: build succeeds with no errors.

- [ ] **Step 4: Run the full native suite once more**

Run: `pio test -e native`
Expected: all binaries still pass (this task touches no native-tested code, but confirms nothing else broke).

- [ ] **Step 5: Commit**

Use the `arlo-commits` skill. Expected classification: `! F Wire identify short-press to republish all turnout status` (real behavior change, Arduino-only code has no native test, so it can't be `^`/`.` even though it's a small, build-verified diff — per this repo's risk ladder, no matching *test* means `!` regardless of line count).

---

## Post-Implementation

- [ ] Update `docs/task-status.md`: move this feature from (implicit) backlog into the Completed table, referencing the commits from Tasks 1–3 and noting the on-device behavior (short BOOT press now both blinks the id and republishes all 8 turnouts' current MQTT status). Follow the existing table's terse, commit-hash-referencing style.
