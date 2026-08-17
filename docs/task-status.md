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
| Add TurnoutCommandSink/TurnoutRegistry implementation plan | ✅ Done | Commit `72f12be`. |
| `TurnoutCommandSink` port + `TurnoutRegistry` domain class (Build Order 9) | ✅ Done | Commit `656dfc8`. First driving-side port (adapters call in); `TurnoutRegistry` owns a fixed `std::array<Turnout, 8>`, buffers commands in `command()`, drains + fans out in `tick()`. Ownership arithmetic (`id/100==nodeId`, index=`id%100-1`) hand-traced against real `Turnout`/`TurnoutMotion`/`FeedbackSensor` by review, zero Critical/Important findings. No `NodeId` value object yet — needs-driven, plain `int nodeId`. |
| Add TopicScheme/PayloadCodec implementation plan | ✅ Done | Commit `52f3921`. |
| `TopicScheme` + `PayloadCodec` domain classes (Build Order 10) | ✅ Done | Merge commit `d25f8a8` (branch `feature/topic-scheme-payload-codec`, commits `06b0332`/`0468ca8`). Pure, stateless, static-method classes — topic format `track/turnout/<id>` picked as a provisional resolution of open item 10.4 ("same, or split?"). Review: ready to merge, one non-blocking Important finding (plan-inherited) — `TopicScheme::parse` doesn't guard `std::stoi` overflow on a long numeric suffix; inert today (no consumer yet), revisit when `MqttCommandSource` (Build Order 11) is built. |
| Add NodeConfig/ConfigStore implementation plan | ✅ Done | Commit `a0b127d`. |
| `NodeId`/`WifiCredentials`/`BrokerAddress`/`TurnoutConfig`/`NodeConfig`/`ConfigStore` (Node Configuration & Commissioning groundwork) | ✅ Done | Merge commit `d5b2f1a` (branch `feature/node-config`, commits `9d2da6d`..`f42e7b2`, 6 tasks). `NodeConfig` composes the first four; `with...()` updates return modified copies (verified non-mutating); `factoryDefault()` deliberately fails `validate()` (`NodeId(0)`, sentinel `-1` pins) to force commissioning; `validate()` checks node-id range + cross-turnout pin conflicts only, needs-driven. `ConfigStore`/`FakeConfigStore` persist `NodeConfig` (not a single `TurnoutConfig`). Opus review independently re-derived the `Orientation`-equality-via-`toLevel` completeness claim, the pin-conflict counting algorithm, and `factoryDefault()`'s self-consistency — zero Critical/Important findings. |
| `ArduinoClock`/`EspDigitalOutput`/`EspDigitalInput`/`NvsConfigStore`/`WiFiLink`/`MqttLink`/`MqttCommandSource`/`MqttPositionReporter` (Build Order 11 — ESP32 adapters) | ✅ Done | Merge commit `c021286` (branch `feature/esp32-adapters`, 12 commits). Verified per-adapter via a build-check cycle (`pio run -e esp32dev` with temporary `src/main.cpp` wiring, reverted after) since there's no native equivalent for hardware-bound code; caught two real bugs this way (`MqttLink::connected()` needed to drop `const` — `PubSubClient::connected()` isn't const-qualified; `esp32dev` was silently building as `gnu++11` with no `-std=` override, fixed by adding `-std=gnu++17`). Also closed two scaffolding-debt items as prep: removed the unused `PwmOutput` port/fake, and guarded `TopicScheme::parse`'s `std::stoi` against `std::out_of_range` on an oversized numeric suffix. `src/main.cpp` is still the no-op composition-root stub — real wiring is backlog #16. |
| `ControllerNode` (Build Order 12) | ✅ Done | Commit `e24a465`. Wires the object graph at startup; `begin()`/`tick()` only, no blocking delays. The static-object exception documented in the architecture. |
| Bench serial commissioning (Node Configuration & Commissioning) | ✅ Done | Commits `71b967e` (`ParsedCommand`), `da837bd` (`CommandLineParser`), `b53c1ab` (`UartPort`/`FakeUartPort`), `4e76a38` (`CommissioningSession`), `ed88144` (`SerialCommissioningAdapter`), `136a81d` (`EspUartPort`). Implements full command set: `id`/`wifi`/`broker`/`turnout`/`show`/`save`/`reboot`. `SerialCommissioningAdapter`/`EspUartPort` not yet wired into `ControllerNode`/`main.cpp` (mirrors #15→#16 split — no boot-mode-selection logic exists yet to decide when bench-commissioning should run instead of normal operation). |
| Wireless commissioning (Wireless Commissioning & Field Identification) | ✅ Done | Commits `4558481` (`MacAddress`), `180d21f` (`SetupApName`), `febe396` (`SetupModeTrigger`/`DeviceIdentity` ports), `8ec7951` (`ButtonSetupModeTrigger`), `f5706fe` (`WebFormCommissioningAdapter`), `3d4b063` (`EspDeviceIdentity`/`CaptivePortalServer`). Reuses `CommandLineParser`'s `ParsedCommand` + `CommissioningSession` for web form parsing. `ButtonSetupModeTrigger`/`WebFormCommissioningAdapter`/`EspDeviceIdentity`/`CaptivePortalServer` not yet wired into `ControllerNode`/`main.cpp` (no boot-mode-selection logic exists yet to decide when setup mode should run instead of normal operation). |

Build Order steps 1–11 (`docs/software-class-list.md`) are complete, plus the
Node Configuration & Commissioning groundwork (`NodeConfig`/`ConfigStore`)
and composition root wiring (`ControllerNode`/`main.cpp`), bench serial
commissioning domain and adapter classes, and wireless commissioning domain and adapter classes.
35 native test binaries pass as of this snapshot (adds `test_parsed_command`,
`test_command_line_parser`, `test_fake_uart_port`, `test_commissioning_session`,
`test_serial_commissioning_adapter` from bench serial, plus `test_mac_address`,
`test_setup_ap_name`, `test_setup_mode_trigger_fakes`, `test_button_setup_mode_trigger`,
`test_web_form_commissioning_adapter` from wireless commissioning — `test_esp32_build_check`,
`EspUartPort`, `EspDeviceIdentity`, and `CaptivePortalServer` are build-check-only, not native binaries).

## Backlog (not started)

Chained in dependency order — a task is only actionable once everything it's
blocked by is done.

| # | Task | Status | Blocked by |
|---|---|---|---|
| 20 | Field identification + duplicate node ID detection (Wireless Commissioning & Field Identification) | ⬜ Pending | — |

### Task details

**#20 — Field identification + duplicate node ID detection (Wireless Commissioning & Field Identification)**
`IdentifyRequestTrigger` port + `ButtonIdentifyRequestTrigger` adapter (BOOT
short-press), `BlinkOutIdentifier` domain class (`NodeId`+`Clock`→blink
`Level` sequence, testable with `ManualClock`). `NodePresenceReporter` port +
`MqttNodePresenceReporter` adapter (retained `node/<id>/status`),
`NodeIdCollisionGuard` domain class (collision vs self vs unrelated, pure).

## Known scaffolding debt

- `ControllerNode`'s debounce/retry durations (`kFeedbackDebounceMs = 20`,
  `kLinkRetryMs = 5000`) are private literals, not `NodeConfig` fields —
  revisit only if a real need for per-node tuning shows up.
- `ControllerNode` assumes an already-commissioned `NodeConfig`; on a
  factory-default board it constructs adapters against pin `-1`, which is
  untested — not reachable without bench commissioning (task #18)
  writing real values first.
- `lib/McsCore/src/adapters/EspUartPort.h` (task #18) has no ongoing
  build-check coverage — `lib_ldf_mode = deep+` does not force-compile a
  header that nothing currently `#include`s, so once task #18's temporary
  `main.cpp` wiring was reverted, the header stopped being compiled by
  `pio run -e esp32dev` at all. The header was genuinely compiled correctly
  during task #18's temporary-wiring step and is correct today — this is a
  coverage gap for *future* edits to this file, not a present defect. Will
  be resolved once something actually wires `EspUartPort` into
  `ControllerNode`/`main.cpp` for real (not part of current plan).
- `ButtonSetupModeTrigger`, `WebFormCommissioningAdapter`, `EspDeviceIdentity`,
  and `CaptivePortalServer` (task #19) are not yet wired into
  `ControllerNode`/`src/main.cpp` — no boot-mode-selection logic exists yet
  to decide when setup mode should run instead of normal operation. Will be
  resolved once the boot-sequence logic is implemented and wired into the
  composition root (not part of current plan).
