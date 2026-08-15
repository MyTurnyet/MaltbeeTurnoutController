# Turnout Value Objects & Debouncer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the firmware its first real domain behavior — stable, debounced feedback-pin readings — by building the `Duration`/`Instant`/`Level` value objects the design calls for, migrating the existing `Clock` port onto `Instant`, and implementing `Debouncer`, the first class in `docs/software-class-list.md`'s Build Order that does actual turnout-relevant logic.

**Architecture:** Hexagonal architecture, matching the rest of this repo. `Duration`, `Instant`, and `Level` are dependency-free immutable value objects living in `lib/McsCore/src/domain/`. `Debouncer` is a pure domain class (no ports, no I/O) that composes them. The existing `Clock` port (`lib/McsCore/src/ports/Clock.h`) and its `FakeClock` test double are migrated from a raw `unsigned long` to `Instant`, since every consumer going forward (`Debouncer`, and later `Deadline`/`FeedbackSensor`) is specified in terms of `Instant`/`Duration`, not milliseconds.

**Tech Stack:** PlatformIO `native` environment, C++17, Catch2 3.7.1 (vendored, `test_framework = custom`).

## Global Constraints

- Domain code must compile and run under the `native` PlatformIO environment **without** `Arduino.h`. (`CLAUDE.md`)
- No blocking calls (`delay()`) — timing goes through the `Clock` port. (`CLAUDE.md`)
- No mocking framework — every class here is small enough (1–2 methods, or a handful for `Debouncer`) that hand-written tests exercise it directly; no fake is needed for `Duration`/`Instant`/`Level`/`Debouncer` since none of them depend on a port. (`CLAUDE.md`, `docs/software-class-list.md`)
- TDD throughout: failing native test first, minimal implementation, refactor only while green. (`CLAUDE.md`)
- Classes are built needs-driven, not speculatively. (`CLAUDE.md`) This plan therefore builds only `Duration`, `Instant`, and `Level` from the Build Order's step 1 — **not** `TurnoutPosition`, `TurnoutState`, `Orientation`, or `TurnoutConfig`, none of which `Debouncer` needs. Those wait for the tasks that actually consume them (`FeedbackSensor`, `TurnoutMotion`).
- `native`'s `build_flags` includes `-Ilib/McsCore/src`, required for any test that reaches a `ports/` or `domain/` header only through another header's `#include`. (`CLAUDE.md`) Keep using include paths like `"domain/Instant.h"` and `"ports/Clock.h"`, matching existing files.
- Every step that changes files ends with `pio test -e native` passing before moving on.

---

## Task 1: `Duration` value object

**Files:**
- Create: `lib/McsCore/src/domain/Duration.h`
- Test: `test/test_duration/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `class Duration` with `explicit Duration(unsigned long milliseconds)`, `unsigned long milliseconds() const`, and `==`, `!=`, `<`, `<=`, `>`, `>=`. Task 2 (`Instant`) depends on this exact constructor and `milliseconds()` accessor for its `operator-`/`operator+`.

- [ ] **Step 1: Write the failing test**

Create `test/test_duration/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Duration.h"

TEST_CASE("Duration reports the milliseconds it was constructed with")
{
    Duration duration(150);

    REQUIRE(duration.milliseconds() == 150);
}

TEST_CASE("Durations with equal milliseconds are equal")
{
    REQUIRE(Duration(100) == Duration(100));
    REQUIRE_FALSE(Duration(100) == Duration(200));
    REQUIRE(Duration(100) != Duration(200));
}

TEST_CASE("Durations compare by milliseconds")
{
    REQUIRE(Duration(100) < Duration(200));
    REQUIRE(Duration(100) <= Duration(100));
    REQUIRE(Duration(200) > Duration(100));
    REQUIRE(Duration(100) >= Duration(100));
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_duration`
Expected: FAIL — compile error, `domain/Duration.h` does not exist.

- [ ] **Step 3: Write `Duration`**

Create `lib/McsCore/src/domain/Duration.h`:

```cpp
#pragma once

class Duration
{
public:
    explicit Duration(unsigned long milliseconds) : milliseconds_(milliseconds)
    {
    }

    unsigned long milliseconds() const
    {
        return milliseconds_;
    }

    bool operator==(const Duration& other) const
    {
        return milliseconds_ == other.milliseconds_;
    }

    bool operator!=(const Duration& other) const
    {
        return !(*this == other);
    }

    bool operator<(const Duration& other) const
    {
        return milliseconds_ < other.milliseconds_;
    }

    bool operator<=(const Duration& other) const
    {
        return milliseconds_ <= other.milliseconds_;
    }

    bool operator>(const Duration& other) const
    {
        return milliseconds_ > other.milliseconds_;
    }

    bool operator>=(const Duration& other) const
    {
        return milliseconds_ >= other.milliseconds_;
    }

private:
    unsigned long milliseconds_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_duration`
Expected: PASS — 3 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_duration`, 0 failures.

- [ ] **Step 6: Commit**

```bash
git add lib/McsCore/src/domain/Duration.h test/test_duration/test_main.cpp
git commit -m "Add Duration value object"
```

---

## Task 2: `Instant` value object

**Files:**
- Create: `lib/McsCore/src/domain/Instant.h`
- Test: `test/test_instant/test_main.cpp`

**Interfaces:**
- Consumes: `Duration` from Task 1 — `Duration(unsigned long)`, `Duration::milliseconds() const`.
- Produces: `class Instant` with `explicit Instant(unsigned long milliseconds)`, `Duration operator-(const Instant& earlier) const`, `Instant operator+(const Duration& duration) const`, and `==`, `!=`, `<`, `<=`, `>`, `>=`. Task 4 (`Clock`/`FakeClock` migration) and Task 5 (`Debouncer`) both depend on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_instant/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Instant.h"

TEST_CASE("Subtracting an earlier Instant yields the elapsed Duration")
{
    Instant earlier(100);
    Instant later(350);

    REQUIRE((later - earlier) == Duration(250));
}

TEST_CASE("Adding a Duration to an Instant yields a later Instant")
{
    Instant start(100);

    REQUIRE((start + Duration(50)) == Instant(150));
}

TEST_CASE("Instants with equal milliseconds are equal")
{
    REQUIRE(Instant(100) == Instant(100));
    REQUIRE_FALSE(Instant(100) == Instant(200));
    REQUIRE(Instant(100) != Instant(200));
}

TEST_CASE("Instants compare by milliseconds")
{
    REQUIRE(Instant(100) < Instant(200));
    REQUIRE(Instant(100) <= Instant(100));
    REQUIRE(Instant(200) > Instant(100));
    REQUIRE(Instant(100) >= Instant(100));
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_instant`
Expected: FAIL — compile error, `domain/Instant.h` does not exist.

- [ ] **Step 3: Write `Instant`**

Create `lib/McsCore/src/domain/Instant.h`:

```cpp
#pragma once

#include "domain/Duration.h"

class Instant
{
public:
    explicit Instant(unsigned long milliseconds) : milliseconds_(milliseconds)
    {
    }

    Duration operator-(const Instant& earlier) const
    {
        return Duration(milliseconds_ - earlier.milliseconds_);
    }

    Instant operator+(const Duration& duration) const
    {
        return Instant(milliseconds_ + duration.milliseconds());
    }

    bool operator==(const Instant& other) const
    {
        return milliseconds_ == other.milliseconds_;
    }

    bool operator!=(const Instant& other) const
    {
        return !(*this == other);
    }

    bool operator<(const Instant& other) const
    {
        return milliseconds_ < other.milliseconds_;
    }

    bool operator<=(const Instant& other) const
    {
        return milliseconds_ <= other.milliseconds_;
    }

    bool operator>(const Instant& other) const
    {
        return milliseconds_ > other.milliseconds_;
    }

    bool operator>=(const Instant& other) const
    {
        return milliseconds_ >= other.milliseconds_;
    }

private:
    unsigned long milliseconds_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_instant`
Expected: PASS — 4 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_duration` and `test_instant`, 0 failures.

- [ ] **Step 6: Commit**

```bash
git add lib/McsCore/src/domain/Instant.h test/test_instant/test_main.cpp
git commit -m "Add Instant value object"
```

---

## Task 3: `Level` value object

**Files:**
- Create: `lib/McsCore/src/domain/Level.h`
- Test: `test/test_level/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `enum class Level { Low, High };`. Task 5 (`Debouncer`) depends on this exact type and its two values.

- [ ] **Step 1: Write the failing test**

Create `test/test_level/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Level.h"

TEST_CASE("Level values are distinct")
{
    REQUIRE(Level::Low != Level::High);
}

TEST_CASE("Level values are equal to themselves")
{
    REQUIRE(Level::Low == Level::Low);
    REQUIRE(Level::High == Level::High);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_level`
Expected: FAIL — compile error, `domain/Level.h` does not exist.

- [ ] **Step 3: Write `Level`**

Create `lib/McsCore/src/domain/Level.h`:

```cpp
#pragma once

enum class Level
{
    Low,
    High
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_level`
Expected: PASS — 2 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_duration`, `test_instant`, and `test_level`, 0 failures.

- [ ] **Step 6: Commit**

```bash
git add lib/McsCore/src/domain/Level.h test/test_level/test_main.cpp
git commit -m "Add Level value object"
```

---

## Task 4: Migrate `Clock` port and `FakeClock` onto `Instant`

**Files:**
- Modify: `lib/McsCore/src/ports/Clock.h`
- Modify: `test/support/FakeClock.h`
- Modify: `test/test_fake_clock/test_main.cpp`

**Interfaces:**
- Consumes: `Instant`/`Duration` from Tasks 1–2.
- Produces: `class Clock { virtual Instant now() const = 0; };` and `class FakeClock : public Clock` with `Instant now() const override`, `void setNow(Instant instant)`, `void advanceBy(Duration duration)`. This replaces the previous `unsigned long nowMillis()`-based interface. Task 5 (`Debouncer`) does not consume `Clock` directly (it takes `Instant` as a parameter), but every future `Clock` consumer (`Deadline`, `FeedbackSensor`, `ArduinoClock`) depends on this exact `Instant`-based signature — this is Build Order step 2, "`ManualClock` — forces `Clock` to exist" in its real (non-`unsigned long`) shape.

- [ ] **Step 1: Update the test to use the new interface (will fail to compile)**

Replace `test/test_fake_clock/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeClock.h"

TEST_CASE("FakeClock begins at zero")
{
    FakeClock clock;

    REQUIRE(clock.now() == Instant(0));
}

TEST_CASE("setNow sets the reported time")
{
    FakeClock clock;

    clock.setNow(Instant(500));

    REQUIRE(clock.now() == Instant(500));
}

TEST_CASE("advanceBy adds to the reported time")
{
    FakeClock clock;
    clock.setNow(Instant(100));

    clock.advanceBy(Duration(50));

    REQUIRE(clock.now() == Instant(150));
}

TEST_CASE("advanceBy can be called multiple times")
{
    FakeClock clock;

    clock.advanceBy(Duration(10));
    clock.advanceBy(Duration(20));
    clock.advanceBy(Duration(30));

    REQUIRE(clock.now() == Instant(60));
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_fake_clock`
Expected: FAIL — compile error, `Clock`/`FakeClock` still expose `nowMillis()`, not `now()`, and don't know about `Instant`/`Duration`.

- [ ] **Step 3: Update the `Clock` port**

Replace `lib/McsCore/src/ports/Clock.h`:

```cpp
#pragma once

#include "domain/Instant.h"

class Clock
{
public:
    virtual ~Clock() = default;
    virtual Instant now() const = 0;
};
```

- [ ] **Step 4: Update `FakeClock`**

Replace `test/support/FakeClock.h`:

```cpp
#pragma once

#include "ports/Clock.h"
#include "domain/Duration.h"

class FakeClock : public Clock
{
public:
    Instant now() const override
    {
        return currentInstant_;
    }

    void setNow(Instant instant)
    {
        currentInstant_ = instant;
    }

    void advanceBy(Duration duration)
    {
        currentInstant_ = currentInstant_ + duration;
    }

private:
    Instant currentInstant_ = Instant(0);
};
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_fake_clock`
Expected: PASS — 4 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — no other test references `Clock::nowMillis()`, so nothing else should break. All tests including `test_fake_clock`, 0 failures.

- [ ] **Step 7: Commit**

```bash
git add lib/McsCore/src/ports/Clock.h test/support/FakeClock.h test/test_fake_clock/test_main.cpp
git commit -m "Migrate Clock port and FakeClock from raw milliseconds to Instant/Duration"
```

---

## Task 5: `Debouncer` domain class

**Files:**
- Create: `lib/McsCore/src/domain/Debouncer.h`
- Test: `test/test_debouncer/test_main.cpp`

**Interfaces:**
- Consumes: `Level` (Task 3), `Instant`/`Duration` (Tasks 1–2).
- Produces: `class Debouncer` with `Debouncer(Level initialLevel, Duration stableDuration)`, `void sample(Level level, Instant now)`, `Level stable() const`. This is the first domain class per `docs/software-class-list.md`'s Build Order (step 3) and has no port dependencies — it's exercised directly in tests, no fake required. Later `FeedbackSensor` composes a `DigitalInput` with a `Debouncer` and an `Orientation` (not built in this plan).

- [ ] **Step 1: Write the failing test**

Create `test/test_debouncer/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Debouncer.h"

TEST_CASE("Debouncer reports the initial level before any samples")
{
    Debouncer debouncer(Level::Low, Duration(50));

    REQUIRE(debouncer.stable() == Level::Low);
}

TEST_CASE("A candidate level that has not persisted for the debounce duration does not become stable")
{
    Debouncer debouncer(Level::Low, Duration(50));

    debouncer.sample(Level::High, Instant(0));
    debouncer.sample(Level::High, Instant(30));

    REQUIRE(debouncer.stable() == Level::Low);
}

TEST_CASE("A candidate level that has persisted for at least the debounce duration becomes stable")
{
    Debouncer debouncer(Level::Low, Duration(50));

    debouncer.sample(Level::High, Instant(0));
    debouncer.sample(Level::High, Instant(50));

    REQUIRE(debouncer.stable() == Level::High);
}

TEST_CASE("A brief glitch that reverts before the debounce duration elapses never becomes stable")
{
    Debouncer debouncer(Level::Low, Duration(50));

    debouncer.sample(Level::High, Instant(0));
    debouncer.sample(Level::Low, Instant(20));
    debouncer.sample(Level::Low, Instant(70));

    REQUIRE(debouncer.stable() == Level::Low);
}

TEST_CASE("A new candidate after becoming stable requires its own full debounce period")
{
    Debouncer debouncer(Level::Low, Duration(50));
    debouncer.sample(Level::High, Instant(0));
    debouncer.sample(Level::High, Instant(50));
    REQUIRE(debouncer.stable() == Level::High);

    debouncer.sample(Level::Low, Instant(60));
    REQUIRE(debouncer.stable() == Level::High);

    debouncer.sample(Level::Low, Instant(110));
    REQUIRE(debouncer.stable() == Level::Low);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_debouncer`
Expected: FAIL — compile error, `domain/Debouncer.h` does not exist.

- [ ] **Step 3: Write `Debouncer`**

Create `lib/McsCore/src/domain/Debouncer.h`:

```cpp
#pragma once

#include "domain/Level.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

class Debouncer
{
public:
    Debouncer(Level initialLevel, Duration stableDuration)
        : stableLevel_(initialLevel),
          candidateLevel_(initialLevel),
          candidateSince_(Instant(0)),
          stableDuration_(stableDuration)
    {
    }

    void sample(Level level, Instant now)
    {
        if (level != candidateLevel_)
        {
            candidateLevel_ = level;
            candidateSince_ = now;
        }

        if (candidateLevel_ != stableLevel_ && (now - candidateSince_) >= stableDuration_)
        {
            stableLevel_ = candidateLevel_;
        }
    }

    Level stable() const
    {
        return stableLevel_;
    }

private:
    Level stableLevel_;
    Level candidateLevel_;
    Instant candidateSince_;
    Duration stableDuration_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_debouncer`
Expected: PASS — 5 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — every test in `test/`, 0 failures.

- [ ] **Step 6: Commit**

```bash
git add lib/McsCore/src/domain/Debouncer.h test/test_debouncer/test_main.cpp
git commit -m "Add Debouncer domain class"
```

---

## Task 6: Update docs to reflect progress

**Files:**
- Modify: `README.md`
- Modify: `CLAUDE.md`

**Interfaces:**
- Consumes: nothing — documentation only.
- Produces: nothing consumed by later tasks.

- [ ] **Step 1: Update `README.md`'s Status section**

In `README.md`, replace the `## Status` section body with:

```markdown
Foundation hardware-abstraction-layer ports are in place, built needs-driven and TDD'd against the native test environment: `Clock`, `DigitalOutput`, `PwmOutput` (`lib/McsCore/src/ports/`), each with a hand-written fake (`test/support/`). The first domain value objects (`Duration`, `Instant`, `Level`) and the first real domain class (`Debouncer`) are also in place under `lib/McsCore/src/domain/`, TDD'd with no test doubles needed. No ESP32 hardware adapters, MQTT/JMRI communication, or turnout-level domain classes (`TurnoutMotion`, `Turnout`) yet — those come next as real needs arise. See `docs/software-class-list.md` for the concrete turnout-controller class breakdown and `docs/superpowers/` for design and implementation history.
```

- [ ] **Step 2: Update `CLAUDE.md`'s "Current source layout" section**

In `CLAUDE.md`, replace:

```markdown
### Current source layout

- `lib/McsCore/src/ports/` — port interfaces (`Clock`, `DigitalOutput`, `PwmOutput`; more added only as a real need arises)
- `lib/McsCore/src/domain/`, `lib/McsCore/src/application/`, `lib/McsCore/src/adapters/` — empty until real classes are needed
- `test/support/` — hand-written fakes (`FakeClock`, `FakeDigitalOutput`, `FakePwmOutput`, ...)
- `test/test_<name>/test_main.cpp` — Catch2 test binaries
```

with:

```markdown
### Current source layout

- `lib/McsCore/src/ports/` — port interfaces (`Clock`, `DigitalOutput`, `PwmOutput`; more added only as a real need arises)
- `lib/McsCore/src/domain/` — value objects and pure domain classes (`Duration`, `Instant`, `Level`, `Debouncer`; more added only as a real need arises, following the Build Order in `docs/software-class-list.md`)
- `lib/McsCore/src/application/`, `lib/McsCore/src/adapters/` — empty until real classes are needed
- `test/support/` — hand-written fakes (`FakeClock`, `FakeDigitalOutput`, `FakePwmOutput`, ...)
- `test/test_<name>/test_main.cpp` — Catch2 test binaries
```

- [ ] **Step 3: Commit**

```bash
git add README.md CLAUDE.md
git commit -m "Update docs after value-object and Debouncer additions"
```
