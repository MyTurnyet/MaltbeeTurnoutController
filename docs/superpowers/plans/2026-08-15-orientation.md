# TurnoutPosition & Orientation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Build Order step 5 in `docs/software-class-list.md` — `Orientation`, which translates between electrical `Level` and the commanded `TurnoutPosition`, both directions, symmetric by construction. `Orientation` needs `TurnoutPosition` to exist first (not yet built), so this plan builds both.

**Architecture:** Both are pure, dependency-free domain value objects, same pattern as `Level`/`Duration`/`Instant`. `TurnoutPosition` is a two-valued type with an `opposite()` method (`docs/software-class-list.md`: "has `opposite()`. The *commanded* concept") — unlike `Level`, which is a bare `enum class` with no methods, `TurnoutPosition` needs a member method, so it's a small class wrapping a private enum, with `closed()`/`thrown()` static factory methods (mirroring `Duration`/`Instant`'s `explicit` constructor + method style) rather than a raw enum. `Orientation` is a similar small class (`normal()`/`inverted()` factories) that composes `Level` and `TurnoutPosition`.

**Tech Stack:** PlatformIO `native` environment, C++17, Catch2 3.7.1 (vendored, `test_framework = custom`).

## Global Constraints

- Domain code must compile and run under the `native` PlatformIO environment **without** `Arduino.h`. (`CLAUDE.md`)
- No mocking framework — neither class has a port dependency. (`CLAUDE.md`)
- TDD throughout: failing native test first — actually run it and see it fail — then minimal implementation, then green. Do not write test and implementation together. (`CLAUDE.md`; this project's `Debouncer` task was flagged in review for skipping genuine RED evidence.)
- Classes are built needs-driven, not speculatively. (`CLAUDE.md`) `TurnoutPosition` gets exactly `closed()`, `thrown()`, `opposite()`, `==`, `!=` — no ordering operators (it's not an ordered type), no `toString()`, no arithmetic. `Orientation` gets exactly `normal()`, `inverted()`, `toLevel(TurnoutPosition) const`, `toPosition(Level) const` — no accessor exposing whether it's inverted, no extra factory helpers.
- **Orientation convention (this plan's design decision, not otherwise specified by the doc):** `normal()` maps `Closed ↔ Level::Low` and `Thrown ↔ Level::High`. `inverted()` maps the opposite: `Closed ↔ Level::High` and `Thrown ↔ Level::Low`. This is an arbitrary but fixed, symmetric, invertible convention — document it here since nothing about "which level means closed" is physically privileged, only that the mapping must round-trip.
- `native`'s `build_flags` includes `-Ilib/McsCore/src` — use include paths like `"domain/Level.h"`, matching existing files.
- Commits in this repo must go through the `/arlo-commits` skill (CLAUDE.md) — never hand-written `git commit`. ACN's "Small Features and Bug Fixes" rule caps any `F`/`B` commit that changes more than 8 lines of code (including tests) at risk level `!` — it cannot be `.`/`^` regardless of test coverage. Both tasks below will be well over 8 lines, so expect `! F` for both.
- Every step that changes files ends with `pio test -e native` passing before moving on.

---

## Task 1: `TurnoutPosition` value object

**Files:**
- Create: `lib/McsCore/src/domain/TurnoutPosition.h`
- Test: `test/test_turnout_position/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `class TurnoutPosition` with `static TurnoutPosition closed()`, `static TurnoutPosition thrown()`, `TurnoutPosition opposite() const`, `bool operator==(const TurnoutPosition&) const`, `bool operator!=(const TurnoutPosition&) const`. Task 2 (`Orientation`) depends on this exact interface — the factory methods are named `closed()`/`thrown()` (lowercase, method call, not `Closed`/`Thrown` static data members — avoids the header-only "static const member of incomplete type" issue and matches the style of `opposite()` as a method).

- [ ] **Step 1: Write the failing test**

Create `test/test_turnout_position/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutPosition.h"

TEST_CASE("closed() and thrown() are distinct")
{
    REQUIRE(TurnoutPosition::closed() != TurnoutPosition::thrown());
}

TEST_CASE("A TurnoutPosition equals itself")
{
    REQUIRE(TurnoutPosition::closed() == TurnoutPosition::closed());
    REQUIRE(TurnoutPosition::thrown() == TurnoutPosition::thrown());
}

TEST_CASE("opposite() of closed is thrown")
{
    REQUIRE(TurnoutPosition::closed().opposite() == TurnoutPosition::thrown());
}

TEST_CASE("opposite() of thrown is closed")
{
    REQUIRE(TurnoutPosition::thrown().opposite() == TurnoutPosition::closed());
}

TEST_CASE("opposite() is its own inverse")
{
    REQUIRE(TurnoutPosition::closed().opposite().opposite() == TurnoutPosition::closed());
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_turnout_position`
Expected: FAIL — compile error, `domain/TurnoutPosition.h` does not exist.

- [ ] **Step 3: Write `TurnoutPosition`**

Create `lib/McsCore/src/domain/TurnoutPosition.h`:

```cpp
#pragma once

class TurnoutPosition
{
public:
    static TurnoutPosition closed()
    {
        return TurnoutPosition(Value::ClosedValue);
    }

    static TurnoutPosition thrown()
    {
        return TurnoutPosition(Value::ThrownValue);
    }

    TurnoutPosition opposite() const
    {
        return TurnoutPosition(value_ == Value::ClosedValue ? Value::ThrownValue : Value::ClosedValue);
    }

    bool operator==(const TurnoutPosition& other) const
    {
        return value_ == other.value_;
    }

    bool operator!=(const TurnoutPosition& other) const
    {
        return !(*this == other);
    }

private:
    enum class Value
    {
        ClosedValue,
        ThrownValue
    };

    explicit TurnoutPosition(Value value) : value_(value)
    {
    }

    Value value_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_turnout_position`
Expected: PASS — 5 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_turnout_position`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill (not a hand-written `git commit`) to commit `lib/McsCore/src/domain/TurnoutPosition.h` and `test/test_turnout_position/test_main.cpp` together. Classify per ACN — new `F` behavior over 8 lines, expect `! F`.

---

## Task 2: `Orientation` value object

**Files:**
- Create: `lib/McsCore/src/domain/Orientation.h`
- Test: `test/test_orientation/test_main.cpp`

**Interfaces:**
- Consumes: `TurnoutPosition` (Task 1) — `static TurnoutPosition closed()`, `static TurnoutPosition thrown()`, `operator==`/`operator!=`. `Level` (already merged) — `enum class Level { Low, High }`.
- Produces: `class Orientation` with `static Orientation normal()`, `static Orientation inverted()`, `Level toLevel(TurnoutPosition position) const`, `TurnoutPosition toPosition(Level level) const`. A later task (`FeedbackSensor`, Build Order step 6) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_orientation/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Orientation.h"

TEST_CASE("Normal orientation maps Closed to Low and Thrown to High")
{
    Orientation orientation = Orientation::normal();

    REQUIRE(orientation.toLevel(TurnoutPosition::closed()) == Level::Low);
    REQUIRE(orientation.toLevel(TurnoutPosition::thrown()) == Level::High);
}

TEST_CASE("Inverted orientation maps Closed to High and Thrown to Low")
{
    Orientation orientation = Orientation::inverted();

    REQUIRE(orientation.toLevel(TurnoutPosition::closed()) == Level::High);
    REQUIRE(orientation.toLevel(TurnoutPosition::thrown()) == Level::Low);
}

TEST_CASE("Normal orientation maps Low back to Closed and High back to Thrown")
{
    Orientation orientation = Orientation::normal();

    REQUIRE(orientation.toPosition(Level::Low) == TurnoutPosition::closed());
    REQUIRE(orientation.toPosition(Level::High) == TurnoutPosition::thrown());
}

TEST_CASE("Inverted orientation maps Low back to Thrown and High back to Closed")
{
    Orientation orientation = Orientation::inverted();

    REQUIRE(orientation.toPosition(Level::Low) == TurnoutPosition::thrown());
    REQUIRE(orientation.toPosition(Level::High) == TurnoutPosition::closed());
}

TEST_CASE("Round-trip through normal orientation returns the original position")
{
    Orientation orientation = Orientation::normal();

    REQUIRE(orientation.toPosition(orientation.toLevel(TurnoutPosition::closed())) == TurnoutPosition::closed());
    REQUIRE(orientation.toPosition(orientation.toLevel(TurnoutPosition::thrown())) == TurnoutPosition::thrown());
}

TEST_CASE("Round-trip through inverted orientation returns the original position")
{
    Orientation orientation = Orientation::inverted();

    REQUIRE(orientation.toPosition(orientation.toLevel(TurnoutPosition::closed())) == TurnoutPosition::closed());
    REQUIRE(orientation.toPosition(orientation.toLevel(TurnoutPosition::thrown())) == TurnoutPosition::thrown());
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_orientation`
Expected: FAIL — compile error, `domain/Orientation.h` does not exist.

- [ ] **Step 3: Write `Orientation`**

Create `lib/McsCore/src/domain/Orientation.h`:

```cpp
#pragma once

#include "domain/Level.h"
#include "domain/TurnoutPosition.h"

class Orientation
{
public:
    static Orientation normal()
    {
        return Orientation(false);
    }

    static Orientation inverted()
    {
        return Orientation(true);
    }

    Level toLevel(TurnoutPosition position) const
    {
        bool isThrown = (position != TurnoutPosition::closed());
        bool isHigh = inverted_ ? !isThrown : isThrown;
        return isHigh ? Level::High : Level::Low;
    }

    TurnoutPosition toPosition(Level level) const
    {
        bool isHigh = (level == Level::High);
        bool isThrown = inverted_ ? !isHigh : isHigh;
        return isThrown ? TurnoutPosition::thrown() : TurnoutPosition::closed();
    }

private:
    explicit Orientation(bool inverted) : inverted_(inverted)
    {
    }

    bool inverted_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_orientation`
Expected: PASS — 6 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — every test in `test/`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill (not a hand-written `git commit`) to commit `lib/McsCore/src/domain/Orientation.h` and `test/test_orientation/test_main.cpp` together. Classify per ACN — new `F` behavior over 8 lines, expect `! F`.

---

## Self-Review Notes (for whoever executes this plan)

- **Spec coverage:** `docs/software-class-list.md`'s `Orientation` entry is fully covered — translates `Level ↔ TurnoutPosition` both directions, and the Build Order's explicit round-trip property (`toPosition(toLevel(p)) == p`) is directly tested for both orientations. `TurnoutPosition`'s entry (`Closed | Thrown`, `opposite()`) is fully covered.
- **No placeholders:** all test and production code above is complete and ready to use verbatim.
- **Type consistency:** Task 2's `Orientation` uses exactly Task 1's `TurnoutPosition::closed()`/`thrown()`/`operator!=` and the already-merged `Level::Low`/`Level::High` — verify `TurnoutPosition.h` from Task 1 hasn't drifted from this signature before starting Task 2.
