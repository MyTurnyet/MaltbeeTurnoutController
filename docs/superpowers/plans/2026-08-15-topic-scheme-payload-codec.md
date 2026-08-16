# TopicScheme & PayloadCodec Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Build Order step 10 in `docs/software-class-list.md` — `TopicScheme` (MQTT topic build/parse) and `PayloadCodec` (payload ↔ domain-value translation). Both are pure, stateless, host-testable domain classes with no network or hardware dependency — the "can be done any time" step in the Build Order.

**Architecture:** Both classes hold zero per-instance state (no fields), so they're implemented with `static` methods only rather than requiring construction — there's nothing to inject or vary between instances. `TopicScheme::topicFor(TurnoutId) -> std::string` / `TopicScheme::parse(const std::string&) -> std::optional<TurnoutId>` build/parse a single shared topic per turnout (`track/turnout/<id>`), used for both the inbound command subscription and the outbound state publish — this resolves the design doc's open item 10.4 ("same, or split?") with "same," the simpler of the two options; nothing downstream is built against this format yet (`MqttCommandSource`/`MqttPositionReporter` are Build Order step 11, not started), so it can be revisited there with zero cost if JMRI's actual configuration wants something different. `PayloadCodec::encode(TurnoutPosition) -> std::string` / `PayloadCodec::decode(const std::string&) -> std::optional<TurnoutPosition>` handle the bidirectional `CLOSED`/`THROWN` ↔ `TurnoutPosition` mapping; `PayloadCodec::encode(TurnoutState) -> std::string` is the one-way outbound mapping (`Closed`→`"CLOSED"`, `Thrown`→`"THROWN"`, `Moving`→`"INCONSISTENT"`, `Unknown`→`"UNKNOWN"`, per the design doc's "What JMRI sees" resolved decision — `TurnoutState::Moving` already covers both the `Moving` and `Settling` internal motion states, so no separate handling is needed here).

**Tech Stack:** PlatformIO `native` environment, C++17 (`std::optional`, `std::string`), Catch2 3.7.1 (vendored, `test_framework = custom`).

## Global Constraints

- Domain code must compile and run under the `native` PlatformIO environment **without** `Arduino.h`. (`CLAUDE.md`)
- No mocking framework — these classes have no ports/dependencies at all, so no test doubles are needed either. (`CLAUDE.md`)
- TDD throughout: failing native test first — actually run it and see it fail — then minimal implementation, then green. Do not write test and implementation together. (`CLAUDE.md`)
- Classes are built needs-driven, not speculatively. (`CLAUDE.md`) **The exact topic string format is provisional** — design doc item 10.4 ("Send/receive MQTT topics: same, or split?") is explicitly still open. This plan picks `track/turnout/<id>` (single shared topic) as a reasonable default since nothing consumes it yet; do not over-invest in configurability (no prefix parameter, no per-node topic namespace) until an adapter actually needs it.
- `native`'s `build_flags` includes `-Ilib/McsCore/src` — use include paths like `"domain/TopicScheme.h"`, matching existing files.
- Commits in this repo must go through the `/arlo-commits` skill (CLAUDE.md) — never hand-written `git commit`. ACN's "Small Features and Bug Fixes" rule caps any `F`/`B` commit that changes more than 8 lines of code (including tests) at risk level `!` — it cannot be `.`/`^` regardless of test coverage. Both tasks are new `F` behavior well over 8 lines, so expect `! F`.
- Every step that changes files ends with `pio test -e native` passing before moving on.
- **Commit often:** each task below ends in its own commit — do not batch Task 1 and Task 2 into a single commit.

---

## Task 1: `TopicScheme` domain class

**Files:**
- Create: `lib/McsCore/src/domain/TopicScheme.h`
- Test: `test/test_topic_scheme/test_main.cpp`

**Interfaces:**
- Consumes: `TurnoutId` (`explicit TurnoutId(int)`, `int value() const`, `operator==`) — already merged.
- Produces: `class TopicScheme` with `static std::string topicFor(TurnoutId id)` and `static std::optional<TurnoutId> parse(const std::string& topic)`. A later task (`MqttCommandSource`/`MqttPositionReporter`, Build Order step 11) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_topic_scheme/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TopicScheme.h"
#include "domain/TurnoutId.h"

TEST_CASE("topicFor builds the topic for a turnout id")
{
    REQUIRE(TopicScheme::topicFor(TurnoutId(103)) == "track/turnout/103");
}

TEST_CASE("parse extracts the turnout id from a well-formed topic")
{
    std::optional<TurnoutId> id = TopicScheme::parse("track/turnout/103");

    REQUIRE(id.has_value());
    REQUIRE(*id == TurnoutId(103));
}

TEST_CASE("parse rejects a topic with the wrong prefix")
{
    REQUIRE_FALSE(TopicScheme::parse("some/other/topic").has_value());
}

TEST_CASE("parse rejects a topic with a non-numeric suffix")
{
    REQUIRE_FALSE(TopicScheme::parse("track/turnout/abc").has_value());
}

TEST_CASE("parse rejects a topic with an empty suffix")
{
    REQUIRE_FALSE(TopicScheme::parse("track/turnout/").has_value());
}

TEST_CASE("parse(topicFor(id)) round-trips back to id")
{
    TurnoutId id(217);

    std::optional<TurnoutId> roundTripped = TopicScheme::parse(TopicScheme::topicFor(id));

    REQUIRE(roundTripped.has_value());
    REQUIRE(*roundTripped == id);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_topic_scheme`
Expected: FAIL — compile error, `domain/TopicScheme.h` does not exist.

- [ ] **Step 3: Write `TopicScheme`**

Create `lib/McsCore/src/domain/TopicScheme.h`:

```cpp
#pragma once

#include <cctype>
#include <optional>
#include <string>

#include "domain/TurnoutId.h"

class TopicScheme
{
public:
    static std::string topicFor(TurnoutId id)
    {
        return kPrefix + std::to_string(id.value());
    }

    static std::optional<TurnoutId> parse(const std::string& topic)
    {
        if (topic.rfind(kPrefix, 0) != 0)
        {
            return std::nullopt;
        }

        std::string suffix = topic.substr(kPrefix.size());

        if (suffix.empty())
        {
            return std::nullopt;
        }

        for (char c : suffix)
        {
            if (!std::isdigit(static_cast<unsigned char>(c)))
            {
                return std::nullopt;
            }
        }

        return TurnoutId(std::stoi(suffix));
    }

private:
    static inline const std::string kPrefix = "track/turnout/";
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_topic_scheme`
Expected: PASS — 6 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — all existing tests plus `test_topic_scheme`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill (not a hand-written `git commit`) to commit `lib/McsCore/src/domain/TopicScheme.h` and `test/test_topic_scheme/test_main.cpp` together. Classify per ACN — new `F` behavior over 8 lines, expect `! F`.

---

## Task 2: `PayloadCodec` domain class

**Files:**
- Create: `lib/McsCore/src/domain/PayloadCodec.h`
- Test: `test/test_payload_codec/test_main.cpp`

**Interfaces:**
- Consumes: `TurnoutPosition` (`closed()`, `thrown()`, `operator==`) and `TurnoutState` (`Closed`, `Thrown`, `Moving`, `Unknown`) — already merged.
- Produces: `class PayloadCodec` with `static std::string encode(TurnoutPosition position)`, `static std::optional<TurnoutPosition> decode(const std::string& payload)`, and `static std::string encode(TurnoutState state)`. A later task (`MqttCommandSource`/`MqttPositionReporter`, Build Order step 11) depends on this exact interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_payload_codec/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/PayloadCodec.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutState.h"

TEST_CASE("encode(TurnoutPosition) maps closed and thrown to their payloads")
{
    REQUIRE(PayloadCodec::encode(TurnoutPosition::closed()) == "CLOSED");
    REQUIRE(PayloadCodec::encode(TurnoutPosition::thrown()) == "THROWN");
}

TEST_CASE("decode maps CLOSED/THROWN payloads back to TurnoutPosition")
{
    std::optional<TurnoutPosition> closed = PayloadCodec::decode("CLOSED");
    std::optional<TurnoutPosition> thrown = PayloadCodec::decode("THROWN");

    REQUIRE(closed.has_value());
    REQUIRE(*closed == TurnoutPosition::closed());
    REQUIRE(thrown.has_value());
    REQUIRE(*thrown == TurnoutPosition::thrown());
}

TEST_CASE("decode rejects an unrecognized payload")
{
    REQUIRE_FALSE(PayloadCodec::decode("bogus").has_value());
}

TEST_CASE("encode(TurnoutState) maps every state to JMRI's expected payload")
{
    REQUIRE(PayloadCodec::encode(TurnoutState::Closed) == "CLOSED");
    REQUIRE(PayloadCodec::encode(TurnoutState::Thrown) == "THROWN");
    REQUIRE(PayloadCodec::encode(TurnoutState::Moving) == "INCONSISTENT");
    REQUIRE(PayloadCodec::encode(TurnoutState::Unknown) == "UNKNOWN");
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_payload_codec`
Expected: FAIL — compile error, `domain/PayloadCodec.h` does not exist.

- [ ] **Step 3: Write `PayloadCodec`**

Create `lib/McsCore/src/domain/PayloadCodec.h`:

```cpp
#pragma once

#include <optional>
#include <string>

#include "domain/TurnoutPosition.h"
#include "domain/TurnoutState.h"

class PayloadCodec
{
public:
    static std::string encode(TurnoutPosition position)
    {
        return (position == TurnoutPosition::closed()) ? "CLOSED" : "THROWN";
    }

    static std::optional<TurnoutPosition> decode(const std::string& payload)
    {
        if (payload == "CLOSED")
        {
            return TurnoutPosition::closed();
        }

        if (payload == "THROWN")
        {
            return TurnoutPosition::thrown();
        }

        return std::nullopt;
    }

    static std::string encode(TurnoutState state)
    {
        switch (state)
        {
        case TurnoutState::Closed:
            return "CLOSED";
        case TurnoutState::Thrown:
            return "THROWN";
        case TurnoutState::Moving:
            return "INCONSISTENT";
        case TurnoutState::Unknown:
            return "UNKNOWN";
        }

        return "UNKNOWN";
    }
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_payload_codec`
Expected: PASS — 4 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — every test in `test/`, 0 failures.

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill (not a hand-written `git commit`) to commit `lib/McsCore/src/domain/PayloadCodec.h` and `test/test_payload_codec/test_main.cpp` together. Classify per ACN — new `F` behavior over 8 lines, expect `! F`.

---

## Self-Review Notes (for whoever executes this plan)

- **Spec coverage:** `docs/software-class-list.md`'s `TopicScheme` (`parse`/`topicFor`) and `PayloadCodec` (`CLOSED`/`THROWN` ↔ `TurnoutPosition`, `TurnoutState` → payload) entries are both covered exactly as scoped. The "What JMRI sees" resolved decision (Moving→INCONSISTENT, Closed/Thrown→CLOSED/THROWN, Faulted/Unknown→UNKNOWN) is covered — note `TurnoutState` has no separate `Faulted` value (it collapses into `Unknown` at the `TurnoutMotion`/`Turnout` boundary, already merged), so `PayloadCodec` only needs to handle the four actual `TurnoutState` values.
- **No placeholders:** all test and production code above is complete and ready to use verbatim.
- **Type consistency:** signatures use exactly the real, already-merged `TurnoutId`/`TurnoutPosition`/`TurnoutState` interfaces — verify none have drifted before starting.
- **Out of scope, deliberately:** `MqttCommandSource`/`MqttPositionReporter` adapters (Build Order step 11, not started) — needs-driven, not speculative. The topic format's provisional status is called out above.
