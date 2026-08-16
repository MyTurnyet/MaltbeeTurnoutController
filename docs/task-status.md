# Task Status

Reference snapshot of implementation tasks against `docs/software-class-list.md`,
generated 2026-08-15. Regenerate/update this file when tasks are completed or
the backlog changes — it's a point-in-time reference, not a live tracker.

## Completed

| Task | Status | Notes |
|---|---|---|
| `Duration` value object | ✅ Done | Commit `ce8957e`. Native-tested, reviewed clean. |
| `Instant` value object | ✅ Done | Commit `0d58e79`. Builds on `Duration`'s real interface. |
| `Level` value object | ✅ Done | Commit `cfbe96a`. Bare `enum class { Low, High }`. |
| Migrate `Clock`/`FakeClock` onto `Instant` | ✅ Done | Commit `7c00aa3`. Replaces raw `unsigned long nowMillis()`. |
| `Debouncer` domain class | ✅ Done | Commit `65e5e4e`. First real domain logic; TDD RED/GREEN verified. |
| Docs: reflect value-object/Debouncer additions | ✅ Done | Commit `9720e30`, merged via `33aa899`. |
| Sync class-list doc with design doc (first pass) | ✅ Done | Commit `49ab3c9` (pre-session) / `c955eed` housekeeping. |
| Sync class-list doc: add commissioning + wireless setup design | ✅ Done | Commit `a55e7db`. |
| `.gitignore`: ignore local git worktree scratch space | ✅ Done | Commit `c955eed`. |
| Add Deadline implementation plan | ✅ Done | Commit `111d4ef`. |
| `Deadline` domain class (Build Order 4) | ✅ Done | Commit `14379a8`. One-shot timer, TDD RED/GREEN verified genuinely. |
| Add TurnoutPosition/Orientation implementation plan | ✅ Done | Commit `2e939e1`. |
| `TurnoutPosition` value object | ✅ Done | Commit `b21b450`. Two-valued (`closed()`/`thrown()`), `opposite()`. Reviewed with zero findings. |
| `Orientation` value object (Build Order 5) | ✅ Done | Commit `e9e9c21`. `Level` ↔ `TurnoutPosition` translation; round-trip property verified by hand and by test. |
| Add DigitalInput/FeedbackSensor implementation plan | ✅ Done | Commit `9b787bc`. |
| `DigitalInput` port + `FakeDigitalInput` | ✅ Done | Commit `5ef2e73`. First new port since the original HAL scaffold; queued-sequence fake. |
| `FeedbackSensor` domain class (Build Order 6) | ✅ Done | Commit `d1aec44`. First class composing a port with domain logic; glitch-rejection test independently hand-traced by review. |
| Add TurnoutState/TurnoutMotion implementation plan | ✅ Done | Commit `642ef7a`. |
| `TurnoutState` value object | ✅ Done | Commit `26521f7`. Bare `enum class { Closed, Thrown, Moving, Unknown }`. |
| `TurnoutMotion` state machine (Build Order 7) | ✅ Done | Commit `64cb81b`. All 15 transition-table tests independently hand-traced against the committed code by an Opus review — zero findings. |
| Add TurnoutId/PositionReporter/DigitalOutput-migration/Turnout plan | ✅ Done | Commit `f526e03`. |
| `TurnoutId` value object | ✅ Done | Commit `ae051b3`. Small wrapper, same style as `Duration`/`Instant`. |
| Migrate `DigitalOutput`/`FakeDigitalOutput` onto `Level` | ✅ Done | Commit `212ab68`. Replaces `set(bool)`/`isSet()` with `write(Level)`/`level()`; closes the last "Known scaffolding debt" item besides unused `PwmOutput`. |
| `PositionReporter` port + `FakePositionReporter` | ✅ Done | Commit `9faa870`. Driven-side port; hand-written fake records `(TurnoutId, TurnoutState)` reports in order. |
| `Turnout` composition class (Build Order 8) | ✅ Done | Commit `45deaa4`. Composes `DigitalOutput&`/`FeedbackSensor`/`TurnoutMotion`/`PositionReporter&` via DI (no `TurnoutConfig` yet — needs-driven, see plan). Reports only on state change; first `tick()` always reports the starting state. |

Build Order steps 1–8 (`docs/software-class-list.md`) are complete. All 17
native test binaries pass as of this snapshot.

## Backlog (not started)

Chained in dependency order — a task is only actionable once everything it's
blocked by is done. `#14` (`TopicScheme`/`PayloadCodec`) and `#17` (`NodeConfig`)
have no blockers and can be picked up any time.

| # | Task | Status | Blocked by |
|---|---|---|---|
| 13 | TurnoutRegistry (Build Order 9) | ⬜ Pending | — (unblocked, #12 done) |
| 14 | TopicScheme + PayloadCodec (Build Order 10) | ⬜ Pending | — |
| 15 | ESP32 adapters (Build Order 11) | ⬜ Pending | #13, #14 |
| 16 | ControllerNode + main.cpp composition root (Build Order 12) | ⬜ Pending | #15 |
| 17 | NodeConfig + ConfigStore migration (Node Configuration & Commissioning) | ⬜ Pending | — |
| 18 | Bench serial commissioning (Node Configuration & Commissioning) | ⬜ Pending | #17 |
| 19 | Wireless commissioning (Wireless Commissioning & Field Identification) | ⬜ Pending | #18 |
| 20 | Field identification + duplicate node ID detection (Wireless Commissioning & Field Identification) | ⬜ Pending | #19 |

### Task details

**#13 — TurnoutRegistry (Build Order 9)**
Owns 8 `Turnout` objects, implements `TurnoutCommandSink`. `command()`
buffers (MQTT callback context safety), `tick()` drains buffer + fans out.
Ownership arithmetic (`id/100==nodeId`, `id%100==channel`).

**#14 — TopicScheme + PayloadCodec (Build Order 10)**
Pure MQTT topic parse/build and CLOSED/THROWN↔`TurnoutPosition` +
`TurnoutState`→payload codec. No network, host-testable. Can be done any
time relative to the other Build Order steps.

**#15 — ESP32 adapters (Build Order 11)**
`ArduinoClock`, `EspDigitalOutput`, `EspDigitalInput`, `NvsConfigStore`,
`WiFiLink`, `MqttLink`, `MqttCommandSource`, `MqttPositionReporter`. Migrate
`DigitalOutput` port to `write(Level)` as part of this (currently
`set(bool)`). Decide fate of unused `PwmOutput`/`FakePwmOutput` port. Only
makes sense once the domain classes above exist and run entirely on native.

**#16 — ControllerNode + main.cpp composition root (Build Order 12)**
Constructs the whole object graph once at startup; `begin()`/`tick()` only,
no `delay()` anywhere. The one documented static-object exception. Last
step, depends on everything above.

**#17 — NodeConfig + ConfigStore migration (Node Configuration & Commissioning)**
`NodeConfig` value object (id, `WifiCredentials`, `BrokerAddress`,
`array<TurnoutConfig,8>`) with `with...()` updates, `factoryDefault()`,
`validate() -> vector<ConfigError>`. `NvsConfigStore` persists `NodeConfig`
instead of a single `TurnoutConfig`. Resolves open item 10.5 groundwork.

**#18 — Bench serial commissioning (Node Configuration & Commissioning)**
`CommandLineParser` (domain, pure, text line→`ParsedCommand`),
`CommissioningSession` (domain, draft `NodeConfig` + `apply(ParsedCommand)` +
`save()`), `SerialCommissioningAdapter` (adapter, `UartPort`→lines→parser→
session). Full command set: `id`/`wifi`/`broker`/`turnout`/`show`/`save`/
`reboot`. `reboot`-not-live-apply per "no dynamic allocation after boot".

**#19 — Wireless commissioning (Wireless Commissioning & Field Identification)**
`SetupModeTrigger` port + `ButtonSetupModeTrigger` adapter (BOOT held at
power-on), `CaptivePortalServer` adapter, `WebFormCommissioningAdapter`
(reuses `CommandLineParser`'s `ParsedCommand` + `CommissioningSession`),
`DeviceIdentity` port + `EspDeviceIdentity` adapter (MAC, setup-AP naming
only). Depends on the bench commissioning domain classes existing first.

**#20 — Field identification + duplicate node ID detection (Wireless Commissioning & Field Identification)**
`IdentifyRequestTrigger` port + `ButtonIdentifyRequestTrigger` adapter (BOOT
short-press), `BlinkOutIdentifier` domain class (`NodeId`+`Clock`→blink
`Level` sequence, testable with `ManualClock`). `NodePresenceReporter` port +
`MqttNodePresenceReporter` adapter (retained `node/<id>/status`),
`NodeIdCollisionGuard` domain class (collision vs self vs unrelated, pure).

## Known scaffolding debt

- `PwmOutput` port (`lib/McsCore/src/ports/PwmOutput.h`) has no consumer
  anywhere in the design and is a removal candidate rather than something to
  keep building on.
