# TurnoutState & TurnoutMotion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Build Order step 7 in `docs/software-class-list.md` — `TurnoutMotion`, the state machine that owns turnout transitions (`AtRest`/`Moving`/`Settling`/`Faulted`) and nothing else. It needs `TurnoutState` (the reported, four-valued concept — not yet built), so this plan builds both.

**Architecture:** `TurnoutState` is a bare `enum class` (no methods needed — same pattern as `Level`), matching the design doc's Value Objects table. `TurnoutMotion` is a pure domain class (no ports) that composes two already-merged `Deadline` instances (movement timeout, settle delay) and tracks a private `MotionState` enum plus the commanded `target_` `TurnoutPosition`. Per the doc's own "plain enum + switch, not the State pattern" decision, transitions are implemented as `switch`/`if` on the private enum — no inheritance, no virtual dispatch. `MotionState` never leaves the class; only `TurnoutState` is exposed via `state() const`, collapsing `Moving`/`Settling` into the single externally-visible `TurnoutState::Moving` per the doc ("Moving vs. Settling is an internal distinction") and `Faulted` into `TurnoutState::Unknown` (per the doc's "Faulted/Unknown publish UNKNOWN").

**Tech Stack:** PlatformIO `native` environment, C++17 (`std::optional`), Catch2 3.7.1 (vendored, `test_framework = custom`).

## Global Constraints

- Domain code must compile and run under the `native` PlatformIO environment **without** `Arduino.h`. (`CLAUDE.md`)
- No mocking framework — `TurnoutMotion` has no port dependency; tested directly, same as `Debouncer`/`Deadline`/`FeedbackSensor`. (`CLAUDE.md`, `docs/software-class-list.md`: "with no ports of its own, the ~15 transition-table tests need zero test doubles")
- TDD throughout: failing native test first — actually run it and see it fail — then minimal implementation, then green. Do not write test and implementation together. (`CLAUDE.md`)
- Classes are built needs-driven, not speculatively. (`CLAUDE.md`) `TurnoutState` gets no methods (bare enum, like `Level`) — the doc doesn't describe any behavior for it beyond holding one of four values. `TurnoutMotion` gets exactly `commandTo(TurnoutPosition, Instant)`, `update(optional<TurnoutPosition>, Instant)`, `state() const` — no accessor exposing the private `MotionState`, no accessor exposing the two `Deadline`s.
- **`TurnoutMotion`'s startup design decision (not otherwise specified by the doc beyond "the transitions"):** the constructor takes an initial `TurnoutPosition` (what the turnout is assumed to be at rest at when the object is constructed — a real board always has *some* physical position at boot, even if software hasn't confirmed it yet) plus the two `Duration`s (movement timeout, settle duration) used to arm the two internal `Deadline`s. It starts in `AtRest` at that position. This is this plan's design decision, not a literal doc quote — document it here so it's not mistaken for something the doc mandated.
- Transition table implemented exactly as `docs/software-class-list.md` specifies:

  | Trigger | From | To |
  |---|---|---|
  | Command received | any | Moving (arm timeout) |
  | Feedback matches target | Moving | Settling (arm settle) |
  | Settle deadline expires | Settling | AtRest |
  | Timeout expires without feedback | Moving | Faulted |
  | Feedback contradicts while at rest | AtRest | Faulted |
  | Feedback matches last target | Faulted | Settling (self-heal) |

  A command arriving in **any** state (including `Settling` and `Faulted`) retargets and re-arms the movement-timeout `Deadline`, per the doc's "Commands during movement: retarget, re-arm the timeout" resolved decision — this plan's tests cover retargeting from `Moving`, `Settling`, and `Faulted`, not just a bare `AtRest → Moving` case.
- `native`'s `build_flags` includes `-Ilib/McsCore/src` — use include paths like `"domain/TurnoutPosition.h"`, matching existing files.
- Commits in this repo must go through the `/arlo-commits` skill (CLAUDE.md) — never hand-written `git commit`. ACN's "Small Features and Bug Fixes" rule caps any `F`/`B` commit that changes more than 8 lines of code (including tests) at risk level `!` — it cannot be `.`/`^` regardless of test coverage. Both tasks below will be well over 8 lines, so expect `! F` for both.
- Every step that changes files ends with `pio test -e native` passing before moving on.

---

## Task 1: `TurnoutState` value object

**Files:**
- Create: `lib/McsCore/src/domain/TurnoutState.h`
- Test: `test/test_turnout_state/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `enum class TurnoutState { Closed, Thrown, Moving, Unknown };`. Task 2 (`TurnoutMotion`) depends on this exact type and its four values.

- [ ] **Step 1: Write the failing test**

Create `test/test_turnout_state/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutState.h"

TEST_CASE("TurnoutState values are distinct")
{
    REQUIRE(TurnoutState::Closed != TurnoutState::Thrown);
    REQUIRE(TurnoutState::Closed != TurnoutState::Moving);
    REQUIRE(TurnoutState::Closed != TurnoutState::Unknown);
    REQUIRE(TurnoutState::Thrown != TurnoutState::Moving);
    REQUIRE(TurnoutState::Thrown != TurnoutState::Unknown);
    REQUIRE(TurnoutState::Moving != TurnoutState::Unknown);
}

TEST_CASE("TurnoutState values are equal to themselves")
{
    REQUIRE(TurnoutState::Closed == TurnoutState::Closed);
    REQUIRE(TurnoutState::Thrown == TurnoutState::Thrown);
    REQUIRE(TurnoutState::Moving == TurnoutState::Moving);
    REQUIRE(TurnoutState::Unknown == TurnoutState::Unknown);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_turnout_state`
Expected: FAIL — compile error, `domain/TurnoutState.h` does not exist.

- [ ] **Step 3: Write `TurnoutState`**

Create `lib/McsCore/src/domain/TurnoutState.h`:

```cpp
#pragma once

enum class TurnoutState
{
    Closed,
    Thrown,
    Moving,
    Unknown
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_turnout_state`
Expected: PASS — 2 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_turnout_state`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill (not a hand-written `git commit`) to commit `lib/McsCore/src/domain/TurnoutState.h` and `test/test_turnout_state/test_main.cpp` together. Classify per ACN — new `F` behavior over 8 lines, expect `! F`.

---

## Task 2: `TurnoutMotion` state machine

**Files:**
- Create: `lib/McsCore/src/domain/TurnoutMotion.h`
- Test: `test/test_turnout_motion/test_main.cpp`

**Interfaces:**
- Consumes: `TurnoutPosition` (already merged) — `closed()`, `thrown()`, `operator==`/`operator!=`. `TurnoutState` (Task 1). `Deadline` (already merged) — `arm(Instant, Duration)`, `disarm()`, `expired(Instant) const`. `Instant`/`Duration` (already merged).
- Produces: `class TurnoutMotion` with `TurnoutMotion(TurnoutPosition initialPosition, Duration movementTimeout, Duration settleDuration)`, `void commandTo(TurnoutPosition position, Instant now)`, `void update(std::optional<TurnoutPosition> observed, Instant now)`, `TurnoutState state() const`. A later task (`Turnout`, Build Order step 8) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_turnout_motion/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutMotion.h"

TEST_CASE("A freshly constructed TurnoutMotion reports the initial position at rest")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));

    REQUIRE(motion.state() == TurnoutState::Closed);
}

TEST_CASE("commandTo transitions from AtRest to Moving")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));

    motion.commandTo(TurnoutPosition::thrown(), Instant(0));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("Feedback matching the target while Moving transitions to Settling, still reported as Moving")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));

    motion.update(TurnoutPosition::thrown(), Instant(10));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("Further updates before the settle deadline elapses keep reporting Moving")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(TurnoutPosition::thrown(), Instant(10));

    motion.update(std::nullopt, Instant(40));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("Once the settle deadline expires, TurnoutMotion returns to AtRest reporting the commanded position")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(TurnoutPosition::thrown(), Instant(10));

    motion.update(std::nullopt, Instant(60));

    REQUIRE(motion.state() == TurnoutState::Thrown);
}

TEST_CASE("If the movement timeout expires before feedback confirms the target, TurnoutMotion faults")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));

    motion.update(std::nullopt, Instant(200));

    REQUIRE(motion.state() == TurnoutState::Unknown);
}

TEST_CASE("A faulted TurnoutMotion self-heals to Settling once feedback matches the last commanded target")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(std::nullopt, Instant(200));

    motion.update(TurnoutPosition::thrown(), Instant(210));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("A faulted TurnoutMotion stays faulted if feedback still does not match the target")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(std::nullopt, Instant(200));

    motion.update(TurnoutPosition::closed(), Instant(210));

    REQUIRE(motion.state() == TurnoutState::Unknown);
}

TEST_CASE("Feedback contradicting the at-rest position faults the motion")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));

    motion.update(TurnoutPosition::thrown(), Instant(5));

    REQUIRE(motion.state() == TurnoutState::Unknown);
}

TEST_CASE("Feedback confirming the at-rest position leaves it at rest")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));

    motion.update(TurnoutPosition::closed(), Instant(5));

    REQUIRE(motion.state() == TurnoutState::Closed);
}

TEST_CASE("No observation while at rest does not fault the motion")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));

    motion.update(std::nullopt, Instant(5));

    REQUIRE(motion.state() == TurnoutState::Closed);
}

TEST_CASE("A new command while already moving retargets and re-arms the movement timeout")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));

    motion.commandTo(TurnoutPosition::closed(), Instant(50));
    motion.update(std::nullopt, Instant(200));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("A retargeted movement faults at the new deadline if feedback still has not arrived")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.commandTo(TurnoutPosition::closed(), Instant(50));

    motion.update(std::nullopt, Instant(250));

    REQUIRE(motion.state() == TurnoutState::Unknown);
}

TEST_CASE("A new command while settling interrupts the settle and returns to Moving toward the new target")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(TurnoutPosition::thrown(), Instant(10));

    motion.commandTo(TurnoutPosition::closed(), Instant(20));

    REQUIRE(motion.state() == TurnoutState::Moving);
}

TEST_CASE("A new command while faulted recovers to Moving toward the new target")
{
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    motion.commandTo(TurnoutPosition::thrown(), Instant(0));
    motion.update(std::nullopt, Instant(200));

    motion.commandTo(TurnoutPosition::closed(), Instant(210));

    REQUIRE(motion.state() == TurnoutState::Moving);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_turnout_motion`
Expected: FAIL — compile error, `domain/TurnoutMotion.h` does not exist.

- [ ] **Step 3: Write `TurnoutMotion`**

Create `lib/McsCore/src/domain/TurnoutMotion.h`:

```cpp
#pragma once

#include <optional>

#include "domain/TurnoutPosition.h"
#include "domain/TurnoutState.h"
#include "domain/Deadline.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

class TurnoutMotion
{
public:
    TurnoutMotion(TurnoutPosition initialPosition, Duration movementTimeout, Duration settleDuration)
        : target_(initialPosition),
          movementTimeout_(movementTimeout),
          settleDuration_(settleDuration),
          motionState_(MotionState::AtRest)
    {
    }

    void commandTo(TurnoutPosition position, Instant now)
    {
        target_ = position;
        motionState_ = MotionState::Moving;
        movementDeadline_.arm(now, movementTimeout_);
        settleDeadline_.disarm();
    }

    void update(std::optional<TurnoutPosition> observed, Instant now)
    {
        switch (motionState_)
        {
        case MotionState::Moving:
            if (observed.has_value() && *observed == target_)
            {
                motionState_ = MotionState::Settling;
                movementDeadline_.disarm();
                settleDeadline_.arm(now, settleDuration_);
            }
            else if (movementDeadline_.expired(now))
            {
                motionState_ = MotionState::Faulted;
            }
            break;

        case MotionState::Settling:
            if (settleDeadline_.expired(now))
            {
                motionState_ = MotionState::AtRest;
                settleDeadline_.disarm();
            }
            break;

        case MotionState::AtRest:
            if (observed.has_value() && *observed != target_)
            {
                motionState_ = MotionState::Faulted;
            }
            break;

        case MotionState::Faulted:
            if (observed.has_value() && *observed == target_)
            {
                motionState_ = MotionState::Settling;
                settleDeadline_.arm(now, settleDuration_);
            }
            break;
        }
    }

    TurnoutState state() const
    {
        switch (motionState_)
        {
        case MotionState::AtRest:
            return (target_ == TurnoutPosition::closed()) ? TurnoutState::Closed : TurnoutState::Thrown;
        case MotionState::Moving:
        case MotionState::Settling:
            return TurnoutState::Moving;
        case MotionState::Faulted:
            return TurnoutState::Unknown;
        }

        return TurnoutState::Unknown;
    }

private:
    enum class MotionState
    {
        AtRest,
        Moving,
        Settling,
        Faulted
    };

    TurnoutPosition target_;
    Duration movementTimeout_;
    Duration settleDuration_;
    MotionState motionState_;
    Deadline movementDeadline_;
    Deadline settleDeadline_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_turnout_motion`
Expected: PASS — 15 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — every test in `test/`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill (not a hand-written `git commit`) to commit `lib/McsCore/src/domain/TurnoutMotion.h` and `test/test_turnout_motion/test_main.cpp` together. Classify per ACN — new `F` behavior well over 8 lines, expect `! F`.

---

## Self-Review Notes (for whoever executes this plan)

- **Spec coverage:** all six transition-table rows in `docs/software-class-list.md` are directly tested: command→Moving (test 2, 12, 14, 15), feedback-matches-target-while-Moving→Settling (test 3), settle-expires→AtRest (test 5), timeout-without-feedback→Faulted (test 6), feedback-contradicts-at-rest→Faulted (test 9), and Faulted self-heal (test 7, 8). The "commands retarget and re-arm" resolved decision is tested from `Moving` (12, 13), `Settling` (14), and `Faulted` (15) — not just the bare `AtRest→Moving` case.
- **No placeholders:** all test and production code above is complete, hand-traced against the implementation, and ready to use verbatim.
- **Type consistency:** `TurnoutMotion` uses exactly `TurnoutState` (Task 1), and the already-merged `TurnoutPosition::closed()`/`operator==`/`operator!=`, `Deadline::arm`/`disarm`/`expired`. Verify none of these have drifted before starting Task 2.
- **Every test case's expected value was hand-traced against the implementation above** during plan authoring (state transitions, deadline arithmetic, and the `target_`-tracking logic) — if an implementer's run disagrees with an expected value here, treat that as a signal to re-check the implementation against this exact code, not to "fix" the test.
