# Identify Short-Press: Republish All Turnout Status — Design

## Context

A short press of BOOT during normal operation (`BootMode::Normal`) already
triggers the field-identification blink-out: `ButtonIdentifyRequestTrigger`
fires, `src/main.cpp` arms a 5s deadline, and `BlinkOutIdentifier` blinks the
node's id on the status LED (GPIO 2). See `docs/software-class-list.md`'s
"Field Identification: Blink-Out" section.

Turnout state is otherwise only published over MQTT when it *changes*
(`Turnout::tick()`, `lib/McsCore/src/domain/Turnout.h`) via the retained
`MqttPositionReporter`. There's currently no way to force a resync of JMRI's
view of a node's turnouts without a state change actually occurring (e.g.
after JMRI restarts and its retained-message view might be stale, or to
manually confirm a node's MQTT link is alive end-to-end).

This adds a second effect to the same short press: republish every turnout's
current status, in addition to the existing identify blink.

## Design

**Same gesture, two effects.** The existing short-press window (50–1500ms)
and `ButtonIdentifyRequestTrigger` are reused as-is — no new physical
gesture, no new trigger class. When it fires, `src/main.cpp` both arms the
identify-blink deadline (existing behavior) and requests a full status
republish (new).

**Domain — force a report, bypassing the on-change guard:**

- `Turnout::reportCurrent()` (`lib/McsCore/src/domain/Turnout.h`) — new
  method. Calls `reporter_.report(id_, motion_.state())` unconditionally
  (unlike `tick()`, which only reports on a state transition) and updates
  `lastReportedState_` to match, so a subsequent real transition still
  reports exactly once rather than being suppressed or double-reported.
- `TurnoutRegistry::reportAll()` (`lib/McsCore/src/domain/TurnoutRegistry.h`)
  — new method. Loops over all 8 `Turnout`s in id order, calling
  `reportCurrent()` on each.

**Adapter/composition wiring:**

- `ControllerNode::republishAllStatus()` (`lib/McsCore/src/adapters/ControllerNode.h`)
  — new public method, forwards to `registry_.reportAll()`.
- `src/main.cpp` — at the existing site where `identifyTrigger->requested()`
  is checked (already inside the `!node->blocked()` branch), call
  `node->republishAllStatus()` alongside the existing
  `identifyDeadline.arm(...)` call.

Reusing the `!node->blocked()` branch means a node that lost the node-id
collision check (and was therefore never subscribed for turnout commands,
per `ControllerNode::begin()`) can't publish status either — consistent with
how that node is already excluded from the identify blink today.

No new ports. This reuses `PositionReporter`/`MqttPositionReporter`
unchanged — each turnout publishes to the same retained topic it always
does; only the trigger condition (forced vs. on-change) differs.

## Testing

- `Turnout::reportCurrent()`: native test with `FakePositionReporter` —
  verify it reports the current state even when unchanged since the last
  report, and that `lastReportedState_` is updated so a later real
  transition still reports exactly once.
- `TurnoutRegistry::reportAll()`: native test with `FakePositionReporter` —
  verify all 8 turnouts report their current state, in id order, regardless
  of whether any had a pending state change.
- `ControllerNode::republishAllStatus()` / `src/main.cpp` wiring: Arduino-only,
  build-check-verified only (`pio run -e esp32dev`) — matches this repo's
  existing convention for composition-root wiring (e.g. the original
  identify-blink wiring itself).

## Out of scope

- No new MQTT topic or combined "snapshot" payload — each turnout still
  publishes individually to its existing per-turnout topic.
- No change to the identify-blink timing/pattern itself.
- No debounce/rate-limit on repeated short presses — same as the existing
  identify blink, a rapid double-press just re-arms/re-publishes; not a
  concern at human button-press cadence.
