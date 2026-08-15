# ESP32 Turnout Controller — Software Class List

> Source of truth: [ESP32 Turnout Controller — Software Class List](https://docs.google.com/document/d/1pA2lRrbFkzAZD5v-xEVw-N_5N47QN5GiCVkWwlXZWMM/edit) (Google Doc). Pulled into the repo on 2026-08-15 so it travels with the code; re-sync this file if the doc changes.

Companion to the project notes and the breadboard prototype doc. This
**supersedes** the earlier generic-HAL draft (`docs/esp32-hal-class-list.md`,
now removed): the actual project is the **ESP32 Turnout Controller**
(WiFi/MQTT/JMRI-integrated driver for Circuitron Tortoise stall motors), and
this file reflects the concrete class breakdown from that design, not a
speculative board-agnostic HAL.

Language/toolchain: C++ / PlatformIO. Domain layer is hardware-free and
compiles on the host via PlatformIO's native environment for fast TDD.

Design principles applied throughout:

- **Layering, one direction.** Domain → Adapters → Composition root.
  Dependencies point inward only. Rule of thumb: if a file includes
  `<Arduino.h>`, it isn't domain code — mechanically checkable in CI.
- **Small interfaces per capability**, not per physical pin. `DigitalOutput`
  knows about voltage levels, not turnouts — all turnout semantics live in
  the domain, where they're testable.
- **Dependency Inversion** — domain code depends on pure virtual interfaces
  only, never directly on Arduino/hardware APIs.
- **"Ask, Don't Tell"** — e.g. `Turnout` doesn't expose state for something
  else to poll and diff; it reports changes outward via `PositionReporter`.
- **Composition over inheritance** — inheritance used only to implement
  interfaces.
- **Immutability** — all value objects are immutable, copyable, trivially
  constructible.
- **No statics**, with one documented exception: a single file-scope
  composition root object in `main.cpp`, required by Arduino's
  `setup()`/`loop()` structure. One static as an entry point is categorically
  different from statics scattered through the domain.
- **No dynamic allocation after boot** — the object graph is built once at
  startup.
- **No mocking frameworks** — every port is small enough that a hand-written
  fake implementing the same interface satisfies it in tests.

---

## Value Objects

Immutable, copyable, trivially constructible. No identity, no behaviour
beyond arithmetic on their own values.

| Type | Holds | Why it exists |
|---|---|---|
| `TurnoutId` | small integer | Type safety — can't be passed where a pin number belongs |
| `TurnoutPosition` | Closed \| Thrown | Two-valued, has `opposite()`. The *commanded* concept |
| `TurnoutState` | Closed \| Thrown \| Moving \| Unknown | Four-valued. The *reported* concept — maps onto JMRI's turnout states |
| `Instant` | milliseconds since boot | Wraps the raw counter so `Clock` has a real return type |
| `Duration` | milliseconds | `Instant - Instant = Duration`. Comparable |
| `Level` | High \| Low | Electrical, not logical — deliberately distinct from `TurnoutPosition` |
| `Orientation` | Normal \| Inverted | Translates between `Level` and `TurnoutPosition`, both directions |
| `TurnoutConfig` | id, output pin, feedback pin, orientation, settle duration, movement timeout | One turnout's whole configuration |

**Why `Position` and `State` are separate types:** you can *command* Closed or
Thrown, but a turnout can *report* Moving or Unknown. Collapsing them into one
enum would force every command site to handle states it can't possibly mean.

**Why `Orientation` is a type rather than a bool:** it owns the translation in
both directions (`toLevel(TurnoutPosition)`, `toPosition(Level)`), so
inversion exists in exactly one place and is symmetric by construction. A
bare bool `inverted` flag tends to get re-tested at every site that touches a
pin.

---

## Ports — Driven Side

Pure virtual interfaces the domain calls outward through.

| Interface | Method(s) | Real implementation | Test double |
|---|---|---|---|
| `Clock` | `Instant now()` | `ArduinoClock` (millis) | `ManualClock` — advances only when told |
| `DigitalOutput` | `void write(Level)` | `EspDigitalOutput` | `RecordingOutput` — remembers writes |
| `DigitalInput` | `Level read()` | `EspDigitalInput` | `ScriptedInput` — returns a queued sequence |
| `PositionReporter` | `void report(TurnoutId, TurnoutState)` | `MqttPositionReporter` | `CapturingReporter` |
| `ConfigStore` | `load()` / `save()` | `NvsConfigStore` | `InMemoryConfigStore` |

**Deliberately dumb ports.** `DigitalOutput` knows about voltage levels, not
turnouts. A port that knew about turnouts would drag domain logic into the
untestable layer. `ManualClock` is what makes TDD viable here — a
three-second settle test runs in microseconds because time only moves when
the test moves it.

**Note:** this table does not include `PwmOutput` — the currently-scaffolded
`lib/McsCore/src/ports/PwmOutput.h` predates this design and isn't part of it.
Tortoise stall motors are driven via simple direction-level `DigitalOutput`,
not PWM speed control. See [Reconciling the scaffolded ports](#reconciling-the-scaffolded-ports-with-this-design) below.

## Ports — Driving Side

| Interface | Method | Implemented by | Called by |
|---|---|---|---|
| `TurnoutCommandSink` | `void command(TurnoutId, TurnoutPosition)` | `TurnoutRegistry` | `MqttCommandSource` |

Only one, intentionally. Every external command path — MQTT now, LCC or
fascia buttons later — arrives through this single door.

---

## Domain Classes

### `Debouncer`

Raw contact reads are noisy. Holds a candidate level and the `Instant` it
first appeared; reports a stable level only after it has persisted.
`void sample(Level, Instant)`, `Level stable() const`. Pure logic, no
dependencies — likely the first class written; it forces `ManualClock` into
existence.

### `FeedbackSensor`

Composes a `DigitalInput`, a `Debouncer`, and an `Orientation`. Turns
electrical reality into a turnout concept. `void sample(Instant)`,
`optional<TurnoutPosition> observed() const`. Returns an optional because no
stable value exists until the debouncer has collected enough samples — this
carries the not-yet-known startup case without a dedicated state. One-sensor
feedback can only distinguish two positions; a jammed machine reads as one of
them (addressed by a future v2.0 two-sensor variant behind the same
interface).

### `Deadline`

A one-shot timer. `void arm(Instant, Duration)` / `void disarm()`,
`bool expired(Instant) const`. Used twice — settle delay and movement
timeout — hence extracted rather than duplicated inline.

### `TurnoutMotion`

The state machine. Owns transitions and nothing else. States: `AtRest`,
`Moving`, `Settling`, `Faulted`.

| Trigger | From | To |
|---|---|---|
| Command received | any | Moving (arm timeout) |
| Feedback matches target | Moving | Settling (arm settle) |
| Settle deadline expires | Settling | AtRest |
| Timeout expires without feedback | Moving | Faulted |
| Feedback contradicts while at rest | AtRest | Faulted |
| Feedback matches last target | Faulted | Settling (self-heal) |

`void commandTo(TurnoutPosition, Instant)`,
`void update(optional<TurnoutPosition> observed, Instant)`,
`TurnoutState state() const`. **Decided separate from `Turnout`** — with no
ports of its own, the ~15 transition-table tests need zero test doubles.
Merged into `Turnout`, each test would need four doubles wired up first.
`MotionState` stays private; only `TurnoutState` is exposed via `state()`
(Moving vs. Settling is an internal distinction).

### `Turnout`

Composes config, output, sensor and motion. Thin by design — mostly
delegation. `void moveTo(TurnoutPosition, Instant)` (writes the level, tells
motion), `void tick(Instant)` (samples sensor, updates motion, reports on
change), `TurnoutId id() const`. Holds a `PositionReporter&` and calls it
only when state changes — never exposes state for external polling.

### `TurnoutRegistry`

Owns all eight `Turnout` objects in a fixed-size array. Implements
`TurnoutCommandSink`. `void command(TurnoutId, TurnoutPosition)` **buffers**
the request rather than applying it immediately — `command()` runs in the
MQTT client's callback context, and doing real work there (GPIO writes,
arming deadlines) invites intermittent faults under network load.
`void tick(Instant)` drains the buffer, then fans out. One optional slot per
turnout; a newer command overwrites the slot, which *is* retargeting, so the
semantics fall out for free. Unknown ids are dropped — ownership is
arithmetic (`id / 100 == nodeId`, `channel id % 100`), a safety net since the
node only subscribes to its own topics anyway.

### `TopicScheme`

Builds and parses MQTT topics. `parse(topic) -> optional<TurnoutId>`,
`topicFor(TurnoutId) -> string`. Pure string work, no network — separated so
topic conventions are testable on the host without a broker.

### `PayloadCodec`

`CLOSED` / `THROWN` ↔ `TurnoutPosition`, and `TurnoutState` → outbound
payload. Also pure.

---

## Adapters

Thin. Any `if` statement here is a smell — it probably belongs in the
domain.

| Class | Wraps | Notes |
|---|---|---|
| `ArduinoClock` | millis | Handle the 49-day rollover, or document that reboots beat it |
| `EspDigitalOutput` | digitalWrite | Sets pinMode in constructor |
| `EspDigitalInput` | digitalRead | Constructor takes whether an internal pull-up is available — GPIO 36/39 have none |
| `NvsConfigStore` | Preferences | Configuration only — position is deliberately not persisted (see below) |
| `WiFiLink` | WiFi association | Non-blocking reconnect |
| `MqttLink` | broker connection | Non-blocking reconnect, sets a Last Will and Testament |
| `MqttCommandSource` | subscription | Parses via `TopicScheme` + `PayloadCodec`, calls `TurnoutCommandSink` |
| `MqttPositionReporter` | publish | Implements `PositionReporter`, sets the retain flag |

**Why no position persistence:** feedback exists, and a remembered value is
strictly worse than a measurement — it's a claim about the past that's wrong
if anything moved while powered down. NVS holds configuration only.

**Why the retain flag matters:** if the node publishes before JMRI connects,
an unretained message is lost and JMRI sits on a stale assumption.

---

## Composition Root

```cpp
class ControllerNode {
public:
    ControllerNode();       // constructs the whole graph
    void begin();
    void tick();
};
```

One file-scope instance in `main.cpp` — the single documented exception to
"no statics." `loop()` calls `tick()` and nothing else. No `delay()` anywhere
in the system.

---

## Test Doubles (no mocking framework)

| Double | Approx. lines | Purpose |
|---|---|---|
| `ManualClock` | ~10 | `advance(Duration)` — makes time a test input |
| `ScriptedInput` | ~15 | Queue of levels to return |
| `RecordingOutput` | ~10 | Last written level, write count |
| `CapturingReporter` | ~15 | Ordered list of reports received |
| `InMemoryConfigStore` | ~15 | Map-backed store |

Under a hundred lines total. If any double starts wanting call-order
assertions or argument matchers, that's the signal the boundary is wrong —
not that a mocking library is needed.

---

## Key Resolved Decisions (context for the class list above)

- **`TurnoutMotion` vs. `Turnout`:** kept separate — test friction, not line
  count, decided it.
- **State representation:** plain enum + switch, not the State pattern. Four
  states map directly onto physical reality and that set won't grow; the
  readability of one file beats extensibility that will never be used.
- **Commands during movement:** retarget, re-arm the timeout, buffer at
  `TurnoutRegistry`. Reversing a Tortoise mid-travel is mechanically
  harmless, so nothing prevents it in software.
- **What JMRI sees:** Moving publishes INCONSISTENT on entry; Settling
  publishes nothing (internal distinction); Closed/Thrown publish
  CLOSED/THROWN; Faulted/Unknown publish UNKNOWN. Published only on
  transition, not per tick.
- **Node identity:** one firmware image for every node; identity set via a
  serial command (id 2) at the bench, stored behind `ConfigStore`. Avoids
  per-node builds and meaningless MAC-derived identity.
- **Turnout numbering:** node-prefixed (Node N owns N01–N16) rather than flat
  blocks — readable without arithmetic, and growth doesn't force
  renumbering.

---

## Build Order

Each step is test-first, everything before it already green:

1. Value objects: `Instant`, `Duration`, `Level`, `TurnoutPosition`, `TurnoutState`
2. `ManualClock` — forces `Clock` to exist
3. `Debouncer` — smallest class with real behaviour
4. `Deadline`
5. `Orientation` — round-trip property: `toPosition(toLevel(p)) == p`
6. `FeedbackSensor` — first class composing a port with domain logic
7. `TurnoutMotion` — one test per transition-table row, including self-heal and retarget/re-arm
8. `Turnout` — composition, report-if-changed
9. `TurnoutRegistry` — buffering, drain-on-tick, ownership arithmetic
10. `TopicScheme`, `PayloadCodec` — pure, can be done any time
11. Everything above runs on the host — only now write adapters
12. `ControllerNode` and `main.cpp`

## Still Open (not yet blocking class design)

| # | Item |
|---|---|
| 10.1 | Confirm JMRI feedback mode is MONITORING, not ONESENSOR, empirically |
| 10.2 | Does a confirming payload cause JMRI to fire listeners twice? |
| 10.3 | Does JMRI accept UNKNOWN as an inbound payload? |
| 10.4 | Send/receive MQTT topics: same, or split? |
| 10.5 | Full serial commissioning command set beyond `id` |
| 10.6 | `millis()` 49-day rollover handling in `ArduinoClock` |

These are answerable via `mosquitto_sub` + the JMRI turnout table, with no
hardware required, and don't change any class in the list above.

---

## Reconciling the scaffolded ports with this design

The HAL-foundation scaffolding work (`docs/superpowers/plans/2026-08-13-hal-foundation-scaffold.md`)
landed three ports before this design doc existed in the repo. Comparing them
against the table above:

- **`Clock`** (`lib/McsCore/src/ports/Clock.h`): currently
  `virtual unsigned long nowMillis() const = 0`. This design wants
  `Instant now()`. `Instant`/`Duration` don't exist yet — introducing them is
  Build Order step 1, and `Clock`/`FakeClock` need a follow-up change to
  return `Instant` instead of a raw `unsigned long` once they do.
- **`DigitalOutput`** (`lib/McsCore/src/ports/DigitalOutput.h`): currently
  `virtual void set(bool state) = 0`. This design wants `void write(Level)`.
  Same story — `Level` is a Build Order step 1 value object; the port
  signature changes once it exists.
- **`PwmOutput`** (`lib/McsCore/src/ports/PwmOutput.h`): **not part of this
  design at all.** Tortoise motors are direction-driven, not speed-driven.
  This port (and `FakePwmOutput`) was built ahead of the concrete domain
  design and currently has no consumer in the class list above. Left in
  place for now since it does no harm sitting unused, but it's a candidate
  for removal rather than something to keep building on.
