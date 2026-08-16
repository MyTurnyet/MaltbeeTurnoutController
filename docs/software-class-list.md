# ESP32 Turnout Controller — Software Class List

> Source of truth: [ESP32 Turnout Controller — Software Class List](https://docs.google.com/document/d/1jbSANH3e8IK4dgGmmz0WCU5wKDMwLRFA2LSdcXLwdoc/edit) (Google Doc). Pulled into the repo on 2026-08-15, re-synced same day to pick up the Node Configuration/Commissioning and Wireless Commissioning sections (superseding an earlier revision at doc id `1pA2lRrbFkzAZD5v-xEVw-N_5N47QN5GiCVkWwlXZWMM`). Re-sync this file if the doc changes again.

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
not PWM speed control. See [Reconciling the scaffolded code with this design](#reconciling-the-scaffolded-code-with-this-design) below.

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
- **Node identity:** one firmware image for every node; identity is a plain
  config field (1–16, chosen from a dropdown during wireless commissioning —
  see [Wireless Commissioning & Field Identification](#wireless-commissioning--field-identification)
  below), stored behind `ConfigStore`. Avoids per-node builds. The device's
  MAC address is used only transiently, to name the temporary setup AP
  before a node has an assigned id. *(Revises the earlier "bench serial `id`
  command" decision — that workflow still exists for bench/manufacturing use,
  see [Node Configuration & Commissioning](#node-configuration--commissioning),
  but is no longer the primary field workflow.)*
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

**Status vs. this Build Order:** steps 1–3 are done (see
[Reconciling the scaffolded code with this design](#reconciling-the-scaffolded-code-with-this-design)).
Step 1 shipped only the two value objects `Debouncer` actually needed
(`Instant`, `Duration`, `Level`) — `TurnoutPosition`/`TurnoutState` are
deferred to whichever of steps 5–9 first needs them, per this project's
needs-driven building principle (`CLAUDE.md`). Steps 4 onward, plus the
Node Configuration/Commissioning and Wireless Commissioning work below, are
not yet started.

## Still Open (not yet blocking class design)

| # | Item |
|---|---|
| 10.1 | Confirm JMRI feedback mode is MONITORING, not ONESENSOR, empirically |
| 10.2 | Does a confirming payload cause JMRI to fire listeners twice? |
| 10.3 | Does JMRI accept UNKNOWN as an inbound payload? |
| 10.4 | Send/receive MQTT topics: same, or split? |
| 10.5 | Full serial commissioning command set beyond `id` — see [Node Configuration & Commissioning](#node-configuration--commissioning) below |
| 10.6 | `millis()` 49-day rollover handling in `ArduinoClock` |

These are answerable via `mosquitto_sub` + the JMRI turnout table, with no
hardware required, and don't change any class in the list above.

---

## Node Configuration & Commissioning

Resolves open item 10.5. Covers how a single firmware image becomes a
*specific* node — its identity, WiFi, broker address, and all eight
`TurnoutConfig`s — and how that configuration gets created and updated in
the field without recompiling.

### New Value Object: `NodeConfig`

```cpp
struct NodeConfig {
    NodeId id;
    WifiCredentials wifi;      // ssid, password
    BrokerAddress broker;      // host, port
    array<TurnoutConfig, 8> turnouts;
};
```

Immutable, like every other value object in this design. Updates go through
`with...()` methods that return a new instance (`withId(NodeId)`,
`withWifi(WifiCredentials)`, `withTurnout(index, TurnoutConfig)`) rather than
mutating in place.

- `NodeConfig::factoryDefault()` — a pure function (not shared mutable
  state, so it doesn't violate "no statics") returning what a brand-new,
  uncommissioned node loads.
- `validate(): vector<ConfigError>` — pure function catching pin conflicts
  (two turnouts claiming GPIO 13), an out-of-range node id, etc., before
  anything is saved. Fully host-testable with no hardware.

This sits directly on top of the existing `ConfigStore` port —
`NvsConfigStore` now persists a `NodeConfig` rather than a single
`TurnoutConfig`.

### Commissioning Classes

Same domain/adapter split as the rest of the system.

| Class | Layer | Responsibility |
|---|---|---|
| `CommandLineParser` | Domain (pure) | Parses a raw text line (e.g. `"turnout 1 pin 13 fb 36 orientation normal"`) into a `ParsedCommand` value. No I/O — trivially host-tested with plain strings in, enum/struct out. |
| `CommissioningSession` | Domain | Holds a draft `NodeConfig`, applies a `ParsedCommand` to produce an updated draft, decides when to call `ConfigStore::save()`. Tested with a scripted list of commands and a `CapturingConfigStore` double — no serial hardware needed. |
| `SerialCommissioningAdapter` | Adapter | Reads bytes off `UartPort`, buffers into lines, hands each line to the parser → session, writes responses back. Thin — just plumbing, per the "any if here is a smell" rule for adapters. |

### Command Set

| Command | Effect |
|---|---|
| `id <n>` | Set node id |
| `wifi <ssid> <password>` | Set WiFi credentials |
| `broker <host> <port>` | Set MQTT broker address |
| `turnout <n> pin <gpio> fb <gpio> orientation <normal\|inverted> settle <ms> timeout <ms>` | Set one turnout's config |
| `show` | Print the current draft config |
| `save` | Persist the draft via `ConfigStore` |
| `reboot` | Restart so the saved config takes effect |

### Why `reboot`, Not Live-Apply

The object graph is built once at startup and never reallocated ("no
dynamic allocation after boot"). Commissioning deliberately does **not**
hot-swap a live `Turnout` or `TurnoutRegistry`. The session only writes a
draft to NVS — nothing takes effect until `ControllerNode`'s constructor
runs again via a soft reboot (`ESP.restart()`), not a live re-wire. This
keeps the composition root's "build once" guarantee intact and avoids an
entire class of "config changed under a running object" bugs.

### Per-Node Workflow

1. Plug into USB, open a serial terminal.
2. `show` — view current config (factory defaults on a fresh board).
3. Set what's needed: `id 3`, `wifi ...`, `broker ...`, `turnout 1 pin 13 fb 36 ...` × 8.
4. `save` — writes the validated `NodeConfig` via `NvsConfigStore`.
5. `reboot` — `ControllerNode` rebuilds from the new config.

One firmware image serves every node; this commissioning step is what
differentiates them — the existing `id 2` example generalized to the whole
config surface, not just node id.

**Note:** the workflow above (USB serial) remains useful at the bench for
manufacturing/testing, but for an end customer with no programming
background, use the wireless workflow below instead — it reuses the same
`CommissioningSession` and `ParsedCommand` domain classes, just fed from a
different adapter.

---

## Wireless Commissioning & Field Identification

Addresses two problems that come up specifically when these nodes are built
by someone else and installed by a non-technical customer:

1. Configuring WiFi/broker/turnout settings without a serial terminal.
2. Telling nodes apart physically once several are installed under the
   layout.

No new hardware is required — this reuses the ESP32 dev board's existing
BOOT button and status LED.

### Entering Setup Mode

Hold **BOOT** while powering on. The node skips normal startup and instead:

1. Starts its own WiFi access point named `Tortoise-Setup-<last 4 hex digits of MAC>`
   (e.g. `Tortoise-Setup-3F2A`). The MAC is used here only because the node
   has no assigned node id yet — it's never used as identity once
   configured.
2. Runs a captive portal: connecting a phone/laptop to that AP auto-opens a
   setup page, same pattern as consumer WiFi devices (Shelly, Tasmota,
   ESPHome, etc.).
3. Serves a form: home WiFi SSID/password, broker address, a **node id
   dropdown (1–16)**, and per-turnout pin/orientation fields (optional if
   the customer is wiring to your standard harness with defaults).
4. On submit, the node validates and reboots into normal operation with the
   new config.

If several new boards are being set up at once, the customer identifies
*which* physical board they're talking to by its blinking "setup mode" LED
and the AP name shown in their WiFi list — both trace back to a specific
board without needing to already know its id.

### New Classes

| Class | Layer | Responsibility |
|---|---|---|
| `SetupModeTrigger` | Port | `bool requested()` — was BOOT held through the boot window? |
| `ButtonSetupModeTrigger` | Adapter | Reads the BOOT pin during `ControllerNode`'s construction |
| `CaptivePortalServer` | Adapter | Runs the AP, DNS capture, and HTTP server; serves the setup page |
| `WebFormCommissioningAdapter` | Adapter | Parses HTTP POST fields into the same `ParsedCommand` values used by `CommandLineParser`, hands them to `CommissioningSession` |
| `DeviceIdentity` | Port | `MacAddress mac()` — read-only hardware identity, used only for setup-AP naming |
| `EspDeviceIdentity` | Adapter | Reads the ESP32's burned-in MAC |

`WebFormCommissioningAdapter` and `SerialCommissioningAdapter` are siblings —
both translate an external input format into `ParsedCommand`s for the same
`CommissioningSession`. The domain doesn't know or care which one was used.

### Field Identification: Blink-Out

A **short press** of BOOT during normal operation (not held through
power-up, so it doesn't trigger setup mode) makes the status LED blink the
node's id N times, then pause and repeat for a few seconds. Lets someone
standing under the layout confirm "this is node 4" without a phone, once a
node has actually been assigned an id.

Reuses the same `SetupModeTrigger` GPIO read, distinguished by hold
duration — short press vs. held-through-boot — so no new physical input is
needed.

| Class | Layer | Responsibility |
|---|---|---|
| `IdentifyRequestTrigger` | Port | `bool requested()` — was BOOT short-pressed this tick? |
| `ButtonIdentifyRequestTrigger` | Adapter | Same physical pin as `ButtonSetupModeTrigger`, different timing window |
| `BlinkOutIdentifier` | Domain | Given a `NodeId` and a `Clock`, produces the on/off `Level` sequence for the LED over time. Pure — testable with `ManualClock`, no LED hardware needed. |

### Duplicate Node ID Detection

A dropdown makes a duplicate id easier to create by accident than the old
rotary switch did (no physical board-by-board differentiation forcing the
installer to notice). Caught via MQTT rather than in the dropdown itself,
since only the broker sees all nodes at once:

- On boot, before fully starting, a node publishes a **retained** presence
  message to `node/<id>/status = online` and **subscribes** to that same
  topic first.
- If a retained message from a *different* boot session already claims that
  id, the node refuses to fully come online and blinks an error pattern
  (distinct from the identify blink) instead.
- This also gives a live "which nodes are online" view for free —
  `mosquitto_sub -t 'node/+/status'` — independent of JMRI.

| Class | Layer | Responsibility |
|---|---|---|
| `NodePresenceReporter` | Port | `void announce(NodeId)` — publish retained presence |
| `MqttNodePresenceReporter` | Adapter | Implements it via `MqttLink`, sets the retain flag |
| `NodeIdCollisionGuard` | Domain | Given the current node's id and an observed presence message, decides collision vs. self vs. unrelated. Pure — testable with plain values, no broker needed. |

### Updated Per-Customer Workflow

1. Power on with BOOT held → node enters setup mode, starts its own AP
   named from its MAC.
2. Customer connects phone to that AP, captive portal opens automatically.
3. Fill in WiFi, broker, node id (1–16 dropdown), turnout settings → submit.
4. Node validates, saves, reboots, announces presence over MQTT (and
   refuses to fully start if another node already claims that id).
5. To confirm identity later: short-press BOOT, count the LED blinks.

No serial terminal, no typing commands, no programming knowledge required.

---

## Reconciling the scaffolded code with this design

The HAL-foundation scaffolding work (`docs/superpowers/plans/2026-08-13-hal-foundation-scaffold.md`)
landed three ports before this design doc existed in the repo, and the
value-objects-and-`Debouncer` plan (`docs/superpowers/plans/2026-08-15-turnout-value-objects-and-debouncer.md`)
has since implemented Build Order steps 1–3. Current state as of this sync:

- **`Clock`** (`lib/McsCore/src/ports/Clock.h`): **migrated**, matches this
  design — `virtual Instant now() const = 0`.
- **`Instant` / `Duration` / `Level`** (`lib/McsCore/src/domain/`):
  **implemented**, matching this design's Value Objects table. Only the two
  value objects `Debouncer` actually needed were built — `TurnoutPosition`
  and `TurnoutState` are intentionally not yet implemented (needs-driven,
  not speculative).
- **`Debouncer`** (`lib/McsCore/src/domain/Debouncer.h`): **implemented**,
  matches this design's signature (`sample(Level, Instant)`, `stable() const`).
- **`Deadline`** (`lib/McsCore/src/domain/Deadline.h`): **implemented**,
  matches this design's signature (`arm(Instant, Duration)`, `disarm()`,
  `expired(Instant) const`). Build Order step 4 done.
- **`TurnoutPosition`** (`lib/McsCore/src/domain/TurnoutPosition.h`):
  **implemented** — `closed()`/`thrown()` factory methods rather than the
  Value Objects table's bare `Closed | Thrown` enum notation, since it needs
  a member method (`opposite()`); matches `Duration`/`Instant`'s
  explicit-construction-plus-methods style.
- **`Orientation`** (`lib/McsCore/src/domain/Orientation.h`): **implemented**,
  matches this design's signature (`toLevel(TurnoutPosition) const`,
  `toPosition(Level) const`, round-trip verified). Build Order step 5 done.
- **`DigitalInput`** (`lib/McsCore/src/ports/DigitalInput.h`): **implemented**,
  matches this design (`Level read()`). Test double is named
  `FakeDigitalInput` (`test/support/FakeDigitalInput.h`), not this doc's
  `ScriptedInput` — same naming divergence already accepted for `Clock`
  (`ManualClock` → `FakeClock`) and `DigitalOutput` (`RecordingOutput` →
  `FakeDigitalOutput`).
- **`FeedbackSensor`** (`lib/McsCore/src/domain/FeedbackSensor.h`):
  **implemented**, matches this design's signature (`sample(Instant)`,
  `optional<TurnoutPosition> observed() const`). Build Order step 6 done —
  first class composing a port with domain logic.
- **`TurnoutState`** (`lib/McsCore/src/domain/TurnoutState.h`): **implemented**
  as a bare `enum class` (like `Level`) — no methods described for it beyond
  holding one of its four values.
- **`TurnoutMotion`** (`lib/McsCore/src/domain/TurnoutMotion.h`):
  **implemented**, matches this design's transition table and signature
  (`commandTo(TurnoutPosition, Instant)`,
  `update(optional<TurnoutPosition>, Instant)`, `state() const`). Build
  Order step 7 done — all 15 transition-table tests independently
  hand-traced against the committed implementation during review.
- **`DigitalOutput`** (`lib/McsCore/src/ports/DigitalOutput.h`): **not yet
  migrated** — still `virtual void set(bool state) = 0`. This design wants
  `void write(Level)`. `Level` now exists (unlike at the last sync), so
  nothing blocks this migration except that no class currently in the repo
  consumes `DigitalOutput` — it'll naturally get migrated alongside whichever
  Build Order step first needs it (`FeedbackSensor` reads via `DigitalInput`
  first; `Turnout`/an ESP32 adapter is the more likely trigger for
  `DigitalOutput`).
- **`PwmOutput`** (`lib/McsCore/src/ports/PwmOutput.h`): **still not part of
  this design.** Tortoise motors are direction-driven, not speed-driven.
  This port (and `FakePwmOutput`) has no consumer anywhere in the class list
  above and remains a candidate for removal rather than something to keep
  building on.
