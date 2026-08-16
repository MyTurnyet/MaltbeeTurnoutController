# DigitalInput Port & FeedbackSensor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Build Order step 6 in `docs/software-class-list.md` — `FeedbackSensor`, the first domain class to compose a port (`DigitalInput`) with existing domain logic (`Debouncer`, `Orientation`), turning electrical reality into a turnout concept. `DigitalInput` doesn't exist in the repo yet, so this plan adds it (plus its test double) first.

**Architecture:** `DigitalInput` is a new driven-side port (`lib/McsCore/src/ports/`), following the exact pattern of the existing `DigitalOutput`/`Clock` ports. Its test double is named `FakeDigitalInput`, matching this repo's established `Fake*` naming (the design doc's own Test Doubles table calls it `ScriptedInput`, but this repo already diverged from the doc's `ManualClock`/`RecordingOutput` naming for `Clock`/`DigitalOutput` without issue — same call here). `FeedbackSensor` is a pure domain class that holds a `DigitalInput&` (injected) plus an `Orientation` and a settle `Duration`, and **lazily constructs its own internal `Debouncer`** on the first `sample()` call, seeded with the first real reading — this sidesteps needing an arbitrary placeholder `Level` at construction time and means the "not yet known" gate uses the exact same `Duration` the `Debouncer` itself debounces with (no risk of two different durations drifting apart).

**Tech Stack:** PlatformIO `native` environment, C++17 (`std::optional` is available), Catch2 3.7.1 (vendored, `test_framework = custom`).

## Global Constraints

- Domain code must compile and run under the `native` PlatformIO environment **without** `Arduino.h`. (`CLAUDE.md`)
- Ports are pure interfaces; adapters (guarded `#ifdef ARDUINO`) come later — this plan only adds the port and its native fake, no ESP32 adapter. (`CLAUDE.md`)
- No mocking framework — `DigitalInput` gets a hand-written `FakeDigitalInput`; `FeedbackSensor` has no port dependency *beyond* the `DigitalInput&` it's given, and is otherwise tested directly like `Debouncer`/`Deadline`. (`CLAUDE.md`)
- TDD throughout: failing native test first — actually run it and see it fail — then minimal implementation, then green. Do not write test and implementation together. (`CLAUDE.md`; `Debouncer` was flagged in review for skipping this once already.)
- Classes are built needs-driven, not speculatively. (`CLAUDE.md`) `DigitalInput` gets exactly `Level read()`. `FeedbackSensor` gets exactly `sample(Instant)`/`observed() const` — no accessor exposing the internal `Debouncer` or raw electrical reading.
- **`FeedbackSensor`'s "not yet known" semantics (this plan's design decision, not otherwise specified by the doc beyond "no stable value exists until the debouncer has collected enough samples"):** `observed()` returns `std::nullopt` until `sample()` has been called at least once **and** at least `stableDuration` has elapsed between the *first* `sample()` call and the most recent one. This is deliberately the same `Duration` used to seed the internal `Debouncer`, so there's exactly one settle-time concept in this class, not two that could disagree.
- `native`'s `build_flags` includes `-Ilib/McsCore/src` — use include paths like `"domain/Level.h"`, `"ports/DigitalInput.h"`, matching existing files.
- Commits in this repo must go through the `/arlo-commits` skill (CLAUDE.md) — never hand-written `git commit`. ACN's "Small Features and Bug Fixes" rule caps any `F`/`B` commit that changes more than 8 lines of code (including tests) at risk level `!` — it cannot be `.`/`^` regardless of test coverage. Both tasks below will be well over 8 lines, so expect `! F` for both.
- Every step that changes files ends with `pio test -e native` passing before moving on.

---

## Task 1: `DigitalInput` port + `FakeDigitalInput`

**Files:**
- Create: `lib/McsCore/src/ports/DigitalInput.h`
- Create: `test/support/FakeDigitalInput.h`
- Test: `test/test_fake_digital_input/test_main.cpp`

**Interfaces:**
- Consumes: `Level` (already merged) — `enum class Level { Low, High }`.
- Produces: `class DigitalInput { virtual Level read() = 0; };` and `class FakeDigitalInput : public DigitalInput` with `void enqueue(Level level)`, `Level read() override`. Task 2 (`FeedbackSensor`) depends on this exact `DigitalInput` interface; its test uses `FakeDigitalInput`.

- [ ] **Step 1: Write the failing test**

Create `test/test_fake_digital_input/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeDigitalInput.h"

TEST_CASE("FakeDigitalInput reads Low by default")
{
    FakeDigitalInput input;

    REQUIRE(input.read() == Level::Low);
}

TEST_CASE("FakeDigitalInput returns enqueued levels in order")
{
    FakeDigitalInput input;
    input.enqueue(Level::High);
    input.enqueue(Level::Low);

    REQUIRE(input.read() == Level::High);
    REQUIRE(input.read() == Level::Low);
}

TEST_CASE("FakeDigitalInput repeats the last read level once the queue is exhausted")
{
    FakeDigitalInput input;
    input.enqueue(Level::High);

    REQUIRE(input.read() == Level::High);
    REQUIRE(input.read() == Level::High);
    REQUIRE(input.read() == Level::High);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_fake_digital_input`
Expected: FAIL — compile error, `support/FakeDigitalInput.h` does not exist (`ports/DigitalInput.h` doesn't exist either).

- [ ] **Step 3: Write the `DigitalInput` port**

Create `lib/McsCore/src/ports/DigitalInput.h`:

```cpp
#pragma once

#include "domain/Level.h"

class DigitalInput
{
public:
    virtual ~DigitalInput() = default;
    virtual Level read() = 0;
};
```

- [ ] **Step 4: Write `FakeDigitalInput`**

Create `test/support/FakeDigitalInput.h`:

```cpp
#pragma once

#include <queue>

#include "ports/DigitalInput.h"

class FakeDigitalInput : public DigitalInput
{
public:
    void enqueue(Level level)
    {
        levels_.push(level);
    }

    Level read() override
    {
        if (levels_.empty())
        {
            return lastLevel_;
        }

        lastLevel_ = levels_.front();
        levels_.pop();
        return lastLevel_;
    }

private:
    std::queue<Level> levels_;
    Level lastLevel_ = Level::Low;
};
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_fake_digital_input`
Expected: PASS — 3 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_fake_digital_input`, 0 failures.

- [ ] **Step 7: Commit**

Use the `/arlo-commits` skill (not a hand-written `git commit`) to commit `lib/McsCore/src/ports/DigitalInput.h`, `test/support/FakeDigitalInput.h`, and `test/test_fake_digital_input/test_main.cpp` together. Classify per ACN — new `F` behavior over 8 lines, expect `! F`.

---

## Task 2: `FeedbackSensor` domain class

**Files:**
- Create: `lib/McsCore/src/domain/FeedbackSensor.h`
- Test: `test/test_feedback_sensor/test_main.cpp`

**Interfaces:**
- Consumes: `DigitalInput&` (Task 1) — `Level read()`. `Debouncer` (already merged) — `Debouncer(Level, Duration)`, `sample(Level, Instant)`, `stable() const`. `Orientation` (already merged) — `toPosition(Level) const`. `TurnoutPosition` (already merged) — `operator==`. `Instant`/`Duration` (already merged).
- Produces: `class FeedbackSensor` with `FeedbackSensor(DigitalInput& input, Orientation orientation, Duration stableDuration)`, `void sample(Instant now)`, `std::optional<TurnoutPosition> observed() const`. A later task (`Turnout`, Build Order step 8) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_feedback_sensor/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/FeedbackSensor.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("FeedbackSensor reports nothing observed before any samples")
{
    FakeDigitalInput input;
    FeedbackSensor sensor(input, Orientation::normal(), Duration(50));

    REQUIRE_FALSE(sensor.observed().has_value());
}

TEST_CASE("FeedbackSensor reports nothing observed before the settle duration has elapsed since the first sample")
{
    FakeDigitalInput input;
    input.enqueue(Level::Low);
    input.enqueue(Level::Low);
    FeedbackSensor sensor(input, Orientation::normal(), Duration(50));

    sensor.sample(Instant(0));
    sensor.sample(Instant(30));

    REQUIRE_FALSE(sensor.observed().has_value());
}

TEST_CASE("FeedbackSensor reports the debounced position, mapped through a normal orientation, once the settle duration has elapsed")
{
    FakeDigitalInput input;
    input.enqueue(Level::Low);
    input.enqueue(Level::Low);
    FeedbackSensor sensor(input, Orientation::normal(), Duration(50));

    sensor.sample(Instant(0));
    sensor.sample(Instant(50));

    REQUIRE(sensor.observed() == TurnoutPosition::closed());
}

TEST_CASE("FeedbackSensor maps through an inverted orientation")
{
    FakeDigitalInput input;
    input.enqueue(Level::High);
    input.enqueue(Level::High);
    FeedbackSensor sensor(input, Orientation::inverted(), Duration(50));

    sensor.sample(Instant(0));
    sensor.sample(Instant(50));

    REQUIRE(sensor.observed() == TurnoutPosition::closed());
}

TEST_CASE("A glitch that reverts before the settle duration elapses does not appear in the observed position")
{
    FakeDigitalInput input;
    input.enqueue(Level::Low);
    input.enqueue(Level::High);
    input.enqueue(Level::Low);
    input.enqueue(Level::Low);
    FeedbackSensor sensor(input, Orientation::normal(), Duration(50));

    sensor.sample(Instant(0));
    sensor.sample(Instant(10));
    sensor.sample(Instant(20));
    sensor.sample(Instant(70));

    REQUIRE(sensor.observed() == TurnoutPosition::closed());
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_feedback_sensor`
Expected: FAIL — compile error, `domain/FeedbackSensor.h` does not exist.

- [ ] **Step 3: Write `FeedbackSensor`**

Create `lib/McsCore/src/domain/FeedbackSensor.h`:

```cpp
#pragma once

#include <optional>

#include "ports/DigitalInput.h"
#include "domain/Debouncer.h"
#include "domain/Orientation.h"
#include "domain/TurnoutPosition.h"
#include "domain/Level.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

class FeedbackSensor
{
public:
    FeedbackSensor(DigitalInput& input, Orientation orientation, Duration stableDuration)
        : input_(input), orientation_(orientation), stableDuration_(stableDuration)
    {
    }

    void sample(Instant now)
    {
        Level level = input_.read();

        if (!debouncer_.has_value())
        {
            debouncer_.emplace(level, stableDuration_);
            firstSampleInstant_ = now;
        }
        else
        {
            debouncer_->sample(level, now);
        }

        lastSampleInstant_ = now;
    }

    std::optional<TurnoutPosition> observed() const
    {
        if (!debouncer_.has_value())
        {
            return std::nullopt;
        }

        if ((lastSampleInstant_ - firstSampleInstant_) < stableDuration_)
        {
            return std::nullopt;
        }

        return orientation_.toPosition(debouncer_->stable());
    }

private:
    DigitalInput& input_;
    Orientation orientation_;
    Duration stableDuration_;
    std::optional<Debouncer> debouncer_;
    Instant firstSampleInstant_ = Instant(0);
    Instant lastSampleInstant_ = Instant(0);
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_feedback_sensor`
Expected: PASS — 5 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — every test in `test/`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill (not a hand-written `git commit`) to commit `lib/McsCore/src/domain/FeedbackSensor.h` and `test/test_feedback_sensor/test_main.cpp` together. Classify per ACN — new `F` behavior over 8 lines, expect `! F`.

---

## Self-Review Notes (for whoever executes this plan)

- **Spec coverage:** `docs/software-class-list.md`'s `FeedbackSensor` entry is fully covered — composes `DigitalInput` + `Debouncer` + `Orientation`, `sample(Instant)`/`optional<TurnoutPosition> observed() const`, and the "no stable value exists until the debouncer has collected enough samples" not-yet-known behavior is directly tested (test cases 1–2). The `DigitalInput` port and `ScriptedInput`-equivalent (`FakeDigitalInput`) from the Ports/Test-Doubles tables are also covered.
- **No placeholders:** all test and production code above is complete and ready to use verbatim.
- **Type consistency:** Task 2's `FeedbackSensor` uses exactly Task 1's `DigitalInput::read()`, plus the already-merged `Debouncer(Level, Duration)`/`sample(Level, Instant)`/`stable() const`, `Orientation::toPosition(Level) const`, and `TurnoutPosition::operator==`. Verify none of these have drifted before starting Task 2.
- **Out of scope, deliberately:** this plan does not address the v2.0 two-sensor jammed-machine variant the doc mentions as future work, and does not touch `DigitalOutput`/adapters — needs-driven, not speculative.
