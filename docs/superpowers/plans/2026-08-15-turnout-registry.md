# TurnoutCommandSink & TurnoutRegistry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Build Order step 9 in `docs/software-class-list.md` — `TurnoutRegistry`, which owns all eight `Turnout` objects for a node, implements the `TurnoutCommandSink` driving-side port, buffers incoming commands (safe to call from an MQTT callback context), and drains + fans out on `tick(Instant)`.

**Architecture:** `TurnoutCommandSink` is a new pure-virtual driving-side port (`void command(TurnoutId, TurnoutPosition)`) — the first port of its kind in this codebase (existing ports are all driven-side: the domain calls out through them). It has no independent testable behavior of its own (no data, no logic), so unlike `DigitalOutput`/`DigitalInput`/`PositionReporter` it is built in the same task as its first real implementer rather than paired with a hand-written fake — there is no consumer yet that would need a fake `TurnoutCommandSink` (that arrives with `MqttCommandSource`, a later Build Order step). `TurnoutRegistry` implements it, owns a fixed `std::array<Turnout, 8>` (moved in via constructor, matching "no dynamic allocation after boot"), and a parallel `std::array<std::optional<TurnoutPosition>, 8> pending_` slot per turnout. `command()` only ever touches `pending_`; `tick()` first drains every armed slot (calling `moveTo` on the matching `Turnout`), then calls `tick()` on all eight `Turnout`s — two separate passes, matching the design doc's "drains the buffer, then fans out" wording.

**Tech Stack:** PlatformIO `native` environment, C++17 (`std::array`, `std::optional`), Catch2 3.7.1 (vendored, `test_framework = custom`).

## Global Constraints

- Domain code must compile and run under the `native` PlatformIO environment **without** `Arduino.h`. (`CLAUDE.md`)
- No mocking framework — hand-written fakes only, reusing already-merged `FakeDigitalOutput`, `FakeDigitalInput`, `FakePositionReporter`. (`CLAUDE.md`)
- TDD throughout: failing native test first — actually run it and see it fail — then minimal implementation, then green. Do not write test and implementation together. (`CLAUDE.md`)
- Classes are built needs-driven, not speculatively. (`CLAUDE.md`) **This plan deliberately does NOT build a `NodeId` value object,** even though the design doc's ownership arithmetic (`id / 100 == nodeId`) implies a node identity concept. `NodeId` belongs to Node Configuration & Commissioning (Build Order step 17, not started) and nothing before that step needs anything beyond a plain integer comparison. `TurnoutRegistry`'s constructor takes `int nodeId` directly — building a whole value-object wrapper now, with no other consumer, would be speculative. When `NodeId` is eventually built, this constructor parameter can be revisited then, needs-driven.
- **Channel-to-array-index convention:** `TurnoutRegistry` stores its eight `Turnout`s in a `std::array<Turnout, 8>` indexed `0..7`. The design doc's ownership arithmetic (`channel = id % 100`) yields a **1-based** channel number (a node's turnouts are numbered 1–8 in the field, matching the silkscreen/wiring convention), so the array index used internally is `channel - 1`. **This plan's responsibility, not enforced by the type system:** whoever builds the `std::array<Turnout, 8>` passed into `TurnoutRegistry`'s constructor (this plan's tests, and later the composition root) must order it so index `i` holds the `Turnout` whose own `TurnoutId` is `nodeId * 100 + (i + 1)`. Getting this wrong silently routes commands to the wrong physical turnout — there is no runtime cross-check between a `Turnout`'s own `id()` and its array position, because `Turnout` doesn't expose enough for `TurnoutRegistry` to verify it cheaply, and adding one now would be speculative given there's exactly one caller (this plan) to get it right.
- Unknown/out-of-range commands (wrong node, or channel outside `1..8`) are silently dropped, per the design doc — this is a safety net, not the primary defense (the node only subscribes to its own MQTT topics), so no logging/error path is needed at this layer.
- `native`'s `build_flags` includes `-Ilib/McsCore/src` — use include paths like `"domain/TurnoutRegistry.h"`, `"ports/TurnoutCommandSink.h"`, matching existing files.
- Commits in this repo must go through the `/arlo-commits` skill (CLAUDE.md) — never hand-written `git commit`. ACN's "Small Features and Bug Fixes" rule caps any `F`/`B` commit that changes more than 8 lines of code (including tests) at risk level `!` — it cannot be `.`/`^` regardless of test coverage. This task is new `F` behavior well over 8 lines, so expect `! F`.
- Every step that changes files ends with `pio test -e native` passing before moving on.

---

## Task 1: `TurnoutCommandSink` port + `TurnoutRegistry` domain class

**Files:**
- Create: `lib/McsCore/src/ports/TurnoutCommandSink.h`
- Create: `lib/McsCore/src/domain/TurnoutRegistry.h`
- Test: `test/test_turnout_registry/test_main.cpp`

**Interfaces:**
- Consumes: `TurnoutId` (`value()`, `explicit TurnoutId(int)`) — already merged. `TurnoutPosition` (`closed()`, `thrown()`) — already merged. `TurnoutState` — already merged. `Turnout` (`Turnout(TurnoutId, DigitalOutput&, Orientation, FeedbackSensor, TurnoutMotion, PositionReporter&)`, `void moveTo(TurnoutPosition, Instant)`, `void tick(Instant)`, `TurnoutId id() const`) — already merged. `Instant` — already merged.
- Produces: `class TurnoutCommandSink { virtual void command(TurnoutId, TurnoutPosition) = 0; };` and `class TurnoutRegistry : public TurnoutCommandSink` with `TurnoutRegistry(int nodeId, std::array<Turnout, 8> turnouts)`, `void command(TurnoutId id, TurnoutPosition position) override` (buffers), `void tick(Instant now)` (drains then fans out). A later task (`MqttCommandSource`, Build Order step 11) depends on the `TurnoutCommandSink` interface; a later task (`ControllerNode`, Build Order step 12) depends on `TurnoutRegistry`'s exact constructor and `tick()` signature.

- [ ] **Step 1: Write the failing test**

Create `test/test_turnout_registry/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <utility>

#include "domain/TurnoutRegistry.h"
#include "domain/Turnout.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutState.h"
#include "domain/Orientation.h"
#include "domain/FeedbackSensor.h"
#include "domain/TurnoutMotion.h"
#include "domain/Duration.h"
#include "domain/Instant.h"
#include "domain/Level.h"
#include "ports/TurnoutCommandSink.h"
#include "ports/DigitalOutput.h"
#include "ports/DigitalInput.h"
#include "ports/PositionReporter.h"
#include "support/FakeDigitalOutput.h"
#include "support/FakeDigitalInput.h"
#include "support/FakePositionReporter.h"

namespace
{
Turnout makeTurnout(int id, DigitalOutput& output, DigitalInput& input, PositionReporter& reporter)
{
    Orientation orientation = Orientation::normal();
    FeedbackSensor sensor(input, orientation, Duration(50));
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    return Turnout(TurnoutId(id), output, orientation, std::move(sensor), std::move(motion), reporter);
}
}

TEST_CASE("A buffered command is applied to the right turnout on tick, by channel arithmetic")
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

    registry.command(TurnoutId(103), TurnoutPosition::thrown());
    registry.tick(Instant(0));

    REQUIRE(outputs[2].level() == Level::High);
    REQUIRE(outputs[3].level() == Level::Low);
}

TEST_CASE("A command for a different node's id is dropped")
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

    registry.command(TurnoutId(201), TurnoutPosition::thrown());
    registry.tick(Instant(0));

    REQUIRE(outputs[0].level() == Level::Low);
}

TEST_CASE("A command with an out-of-range channel is dropped")
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

    registry.command(TurnoutId(109), TurnoutPosition::thrown());
    registry.tick(Instant(0));

    for (const auto& output : outputs)
    {
        REQUIRE(output.level() == Level::Low);
    }
}

TEST_CASE("A newer command overwrites a still-pending slot before tick (retargeting)")
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

    registry.command(TurnoutId(104), TurnoutPosition::thrown());
    registry.command(TurnoutId(104), TurnoutPosition::closed());
    registry.tick(Instant(0));

    REQUIRE(outputs[3].level() == Level::Low);
}

TEST_CASE("tick() with no pending commands still fans out and reports every turnout's initial state")
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

    REQUIRE(reporter.reports().size() == 8);
    REQUIRE(reporter.reports()[0].id == TurnoutId(101));
    REQUIRE(reporter.reports()[0].state == TurnoutState::Closed);
    REQUIRE(reporter.reports()[7].id == TurnoutId(108));
    REQUIRE(reporter.reports()[7].state == TurnoutState::Closed);
}

TEST_CASE("TurnoutRegistry satisfies TurnoutCommandSink through a base-class reference")
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
    TurnoutCommandSink& sink = registry;

    sink.command(TurnoutId(105), TurnoutPosition::thrown());
    registry.tick(Instant(0));

    REQUIRE(outputs[4].level() == Level::High);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_turnout_registry`
Expected: FAIL — compile error, `domain/TurnoutRegistry.h` and `ports/TurnoutCommandSink.h` do not exist.

- [ ] **Step 3: Write the `TurnoutCommandSink` port**

Create `lib/McsCore/src/ports/TurnoutCommandSink.h`:

```cpp
#pragma once

#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"

class TurnoutCommandSink
{
public:
    virtual ~TurnoutCommandSink() = default;
    virtual void command(TurnoutId id, TurnoutPosition position) = 0;
};
```

- [ ] **Step 4: Write `TurnoutRegistry`**

Create `lib/McsCore/src/domain/TurnoutRegistry.h`:

```cpp
#pragma once

#include <array>
#include <optional>
#include <utility>

#include "domain/Turnout.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/Instant.h"
#include "ports/TurnoutCommandSink.h"

class TurnoutRegistry : public TurnoutCommandSink
{
public:
    static constexpr int TurnoutsPerNode = 8;

    TurnoutRegistry(int nodeId, std::array<Turnout, TurnoutsPerNode> turnouts)
        : nodeId_(nodeId), turnouts_(std::move(turnouts))
    {
    }

    void command(TurnoutId id, TurnoutPosition position) override
    {
        int value = id.value();
        int owningNode = value / 100;
        int channel = value % 100;

        if (owningNode != nodeId_)
        {
            return;
        }

        if (channel < 1 || channel > TurnoutsPerNode)
        {
            return;
        }

        pending_[channel - 1] = position;
    }

    void tick(Instant now)
    {
        for (int i = 0; i < TurnoutsPerNode; ++i)
        {
            if (pending_[i].has_value())
            {
                turnouts_[i].moveTo(*pending_[i], now);
                pending_[i].reset();
            }
        }

        for (auto& turnout : turnouts_)
        {
            turnout.tick(now);
        }
    }

private:
    int nodeId_;
    std::array<Turnout, TurnoutsPerNode> turnouts_;
    std::array<std::optional<TurnoutPosition>, TurnoutsPerNode> pending_;
};
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_turnout_registry`
Expected: PASS — 6 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — every test in `test/`, 0 failures.

- [ ] **Step 7: Commit**

Use the `/arlo-commits` skill (not a hand-written `git commit`) to commit `lib/McsCore/src/ports/TurnoutCommandSink.h`, `lib/McsCore/src/domain/TurnoutRegistry.h`, and `test/test_turnout_registry/test_main.cpp` together. Classify per ACN — new `F` behavior well over 8 lines, expect `! F`.

---

## Self-Review Notes (for whoever executes this plan)

- **Spec coverage:** `docs/software-class-list.md`'s `TurnoutRegistry` entry is covered — fixed-size ownership of eight `Turnout`s, `TurnoutCommandSink` implementation, buffer-then-drain-on-tick semantics, one optional slot per turnout (retargeting falls out for free), unknown ids dropped via `id / 100 == nodeId` / `id % 100` channel arithmetic. The `TurnoutCommandSink` port from the Ports — Driving Side table is covered with its exact signature.
- **No placeholders:** all test and production code above is complete, hand-verified against the already-merged `Turnout`/`TurnoutMotion`/`FeedbackSensor`/`Orientation` implementations (each test case's expected output/report was traced by hand through those collaborators during plan authoring, reusing the same reasoning already verified in the `Turnout` plan), and ready to use verbatim.
- **Type consistency:** `TurnoutRegistry`'s constructor and method signatures use exactly the real, already-merged interfaces of `Turnout`/`TurnoutId`/`TurnoutPosition`/`Instant` — verify none have drifted before starting.
- **Out of scope, deliberately:** `NodeId` value object (see Global Constraints), `MqttCommandSource` adapter and a hand-written fake `TurnoutCommandSink` test double (needed only once that adapter is built, Build Order step 11) — needs-driven, not speculative.
