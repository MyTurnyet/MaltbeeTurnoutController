# MQTT Turnout-Command Activity LED — Design

## Purpose

Give a customer/builder a way to visually confirm, standing at the board,
that JMRI's turnout commands are actually reaching a given node over MQTT —
useful for troubleshooting "why isn't this turnout responding" without a
laptop or serial connection. The status LED (GPIO 2) flashes three times,
quickly, each time the node successfully decodes and acts on an incoming
turnout command.

## Scope decisions

- **Which messages count:** only turnout commands — payloads that
  `PayloadCodec::decode` successfully parses and that get dispatched to the
  `TurnoutRegistry` via `MqttCommandSource::handle`. This node's own
  presence-topic traffic (`node/<id>/status`) does **not** count — it's
  mostly this node's own retained announce/echo, not JMRI activity, and
  isn't useful for "is JMRI reaching my turnouts" troubleshooting. A
  malformed/undecodable payload does not count either — it didn't actually
  command a turnout, so flashing for it would be misleading.
- **Priority vs. existing LED states:** the status LED already has two
  higher-priority uses in `BootMode::Normal` — collision-blink (steady fast,
  when `ControllerNode::blocked()`) and identify-blink (per-node-id pattern,
  for 5s after a short BOOT press). The MQTT-activity flash only shows when
  neither of those is active; it never interrupts them. A command received
  while the LED is busy with collision- or identify-blink is simply not
  shown — no queuing.
- **Flash pattern:** three quick flashes, 80ms on / 80ms off each (~480ms
  total), then dark. A new command arriving mid-burst restarts the burst
  from flash 1 (matches how the identify-blink window already restarts on a
  fresh short-press) rather than queuing or extending.
- **Out of scope:** no per-turnout distinction (the flash doesn't say
  *which* turnout moved, only that a command arrived), no persistence across
  reboot, no MQTT traffic other than turnout commands.

## Components

### `FlashBurst` (new, pure domain class)

`lib/McsCore/src/domain/FlashBurst.h` — same shape and testing story as the
existing `BlinkOutIdentifier`/`SteadyBlinker` (pure `Duration`/`Level` math,
no `Clock`, no hardware, fully native-unit-testable), but one-shot instead
of looping:

```cpp
class FlashBurst
{
public:
    FlashBurst(Duration onDuration, Duration offDuration, int flashCount);
    Level levelAt(Duration elapsed) const;
};
```

`levelAt` returns the on/off pattern for `flashCount` cycles of
`onDuration`+`offDuration`, then `Level::Low` for any elapsed time at or
past the total burst duration (`flashCount * (onDuration + offDuration)`).
Unlike `BlinkOutIdentifier`, there is no pause-and-repeat — once the burst
finishes, it stays dark until something re-triggers it (driven externally by
a `Deadline`, same as the identify pattern already does).

### `MqttCommandSource` (modified)

`lib/McsCore/src/adapters/MqttCommandSource.h` gains:

- A private `bool receivedThisTick_ = false;` flag, set to `true` inside
  `handle()` only on the branch where `PayloadCodec::decode` succeeds and
  `sink_.command(id, *position)` is called.
- `bool consumeReceived()` — returns the flag's current value and clears it
  to `false`. Read-and-clear, same idiom as `SetupModeRequestStore`'s
  `consumeRequest()`.

No native test for this change — `MqttCommandSource` is already
`#ifdef ARDUINO`-guarded with no native test today (it depends on the real
`MqttLink`/`PubSubClient`), so this follows existing precedent: build-check
(`pio run -e esp32dev`) plus on-device verification.

### `ControllerNode` (modified)

`lib/McsCore/src/adapters/ControllerNode.h` gains a one-line passthrough:

```cpp
bool turnoutCommandReceived()
{
    return commandSource_.consumeReceived();
}
```

Called from `main.cpp` once per `loop()` iteration, right after
`node->tick()` — the same spot `node->blocked()` is already checked.

### `src/main.cpp` (modified, composition root only)

Adds:
- Three new constants: `kMqttFlashOnMs = 80`, `kMqttFlashOffMs = 80`,
  `kMqttFlashCount = 3`.
- A file-scope `Deadline mqttActivityDeadline;` and `Instant
  mqttActivityStart(0);` (plain domain value objects — safe as ordinary
  statics, same rule already documented for `identifyDeadline`).
- A file-scope `FlashBurst* mqttFlashBurst = nullptr;` constructed once in
  `setup()`'s `BootMode::Normal` branch, alongside the other Normal-mode-only
  LED-pattern objects (`blinkIdentifier`, `collisionBlinker`).
- In `loop()`'s `node != nullptr` / `!node->blocked()` branch: after the
  existing identify-trigger handling, check `node->turnoutCommandReceived()`;
  if true, arm `mqttActivityDeadline` for the burst's total duration and set
  `mqttActivityStart = now`. Then extend the existing `identifying ? ... :
  Level::Low` ternary into a three-way priority: identifying →
  `blinkIdentifier`; else if `mqttActivityDeadline.armed() &&
  !mqttActivityDeadline.expired(now)` → `mqttFlashBurst->levelAt(now -
  mqttActivityStart)`; else → `Level::Low`.

No changes to `BootMode::WirelessSetup` or `BootMode::NeedsCommissioning`
branches — turnout commands are never subscribed to outside `Normal` mode
(`ControllerNode::begin()` only calls `commandSource_.subscribeAll()` when
`!blocked_`), so there's nothing to flash there.

## Data Flow

```
JMRI publishes turnout command
  → PubSubClient callback (MqttLink constructor's lambda)
  → MqttTopicRouter::dispatch (exact topic match)
  → MqttCommandSource::handle
      → PayloadCodec::decode
      → [on success] sink_.command(id, position); receivedThisTick_ = true
  → (next loop() tick)
  → main.cpp: node->turnoutCommandReceived() → true
  → mqttActivityDeadline.arm(now, burstDuration); mqttActivityStart = now
  → statusLed->write(mqttFlashBurst->levelAt(now - mqttActivityStart))
      (repeated each tick until the burst's own levelAt() settles to Low)
```

## Testing

- **`FlashBurst`**: full native TDD — same test shape as `BlinkOutIdentifier`
  (`test/test_flash_burst/test_main.cpp`): stays off before any elapsed time
  passes a boundary check, correct on/off pattern across all 3 flashes,
  settles to `Level::Low` at and after the total burst duration, and stays
  `Level::Low` well past it.
- **`MqttCommandSource`/`ControllerNode`/`main.cpp`**: no native test
  possible (Arduino-gated / composition root) — verified by
  `pio run -e esp32dev` build-check, then on-device: publish a turnout
  command via MQTT (e.g. `mosquitto_pub` or JMRI) to a configured, running
  board and visually confirm three quick flashes.

## Docs

Add a row to `docs/first-time-setup.md`'s Step 5 LED table:

| LED behavior | Meaning |
|---|---|
| Three quick flashes | Received a turnout command from JMRI — confirms MQTT commands are reaching this board. |
