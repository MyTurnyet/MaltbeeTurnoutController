# ESP32 Hardware Abstraction Layer — Class List

Scope note: this covers the **hardware abstraction layer (HAL)** implied directly by
the ELEGOO ESP32 pinout diagram — the layer that wraps the pins/peripherals shown on
the board. Application-specific classes (e.g. `TemperatureMonitor`,
`RobotArmController`) sit on top of this and depend on the actual project being built.

Design principles applied throughout:

- Small interfaces per **capability**, not per physical pin. A pin's role (e.g. GPIO32
  as touch vs. ADC vs. digital I/O) is a configuration choice, not an inheritance
  hierarchy.
- Dependency Inversion — application code depends on these interfaces, never on
  concrete ESP32 SDK calls directly.
- "Ask, Don't Tell" — objects are asked to do things (`digitalOutput.set(true)`)
  rather than having their internals inspected and acted upon externally.
- Composition over inheritance — no class hierarchies of pin types.
- Immutability — pin identity and capability objects are immutable value objects.
- No statics — everything is constructed and injected via the composition root.
- No mocking frameworks needed — every interface is small enough (1–2 methods) that
  a hand-written in-memory fake satisfies it for tests.

---

## Capability Interfaces (the seams)

| Interface | Responsibility | Wraps |
|---|---|---|
| `DigitalInput` | `read(): Boolean` | GPIO configured as input |
| `DigitalOutput` | `set(state: Boolean)` | GPIO configured as output |
| `AnalogInput` | `readRaw(): Int`, `readVoltage(): Double` | ADC-capable pin — ADC1 channels (always available) or ADC2 channels (unusable whenever Wi-Fi is active — see Global Constraints below) |
| `AnalogOutput` | `write(level: Double)` | DAC pins (GPIO25 / GPIO26) |
| `PwmOutput` | `writeDutyCycle(percent: Double)` | Any pin, routed through a free LEDC channel — see Global Constraints below |
| `TouchInput` | `readTouchValue(): Int`, `isTouched(): Boolean` | TOUCH0–TOUCH9 pins |
| `I2CBus` | `write(address, bytes)`, `read(address, length): ByteArray` | Separate instances for I2C0 and I2C1 (SDA/SCL freely remappable, not fixed to GPIO21/22) |
| `SpiBus` | `transfer(bytes): ByteArray` | Separate instances for HSPI and VSPI |
| `UartPort` | `send(bytes)`, `receive(): ByteArray` | TX0/RX0, TX2/RX2 |
| `Clock` | `nowMillis(): Long` | Not pin-backed — wraps the ESP32's system tick so debounce, pulse timing, and blink logic never call `delay()`/read `millis()` directly |

**Notes:**
- `DigitalOutput`'s pin reference is immutable — only the on-hardware state changes,
  never the object's identity.
- `SpiBus` needs two separate instances/implementations because the board exposes
  both HSPI and VSPI buses independently.
- `I2CBus` needs the same two-instance treatment as `SpiBus` — the ESP32 has two
  independent I2C controllers (I2C0/I2C1), not one fixed SDA/SCL pair.
- `PwmOutput` is backed by a shared pool of LEDC channels (16 total, across two
  timer groups) rather than being a purely per-pin capability — allocating one
  should fail explicitly once the pool is exhausted, the same way pin-capability
  mismatches do.
- `Clock` has no physical pin association. It's included here anyway because it's
  as foundational as `DigitalInput`/`DigitalOutput` for anything with timing —
  debounce, non-blocking pulse-timed drive, blink-on-unknown-state — and keeping it
  out of the domain layer's direct reach is what lets that logic run under `native`
  tests without real delays.
- Each interface stays small enough to keep implementing methods under the 8-line /
  low-cognitive-complexity budget.

---

## Configuration / Identity Classes

### `GpioPinId`
Immutable value object wrapping a validated pin number (0–39). Prevents invalid pins
from being constructed at all — invalid input fails at the boundary, not deep in
application logic.

### `PinCapabilities`
Immutable value object describing what a given `GpioPinId` supports: touch-capable?
ADC channel — and if so, ADC1 or ADC2 (ADC2 channels are unusable whenever the Wi-Fi
radio is active, which is continuous on this board — see Global Constraints below)?
DAC channel? Input-only (e.g. GPIO34–39, which have no output driver)? Boot-sensitive
(GPIO 0, 2, 5, 12, 15 — have special behavior at boot/flash-mode selection and should
be flagged rather than silently allowed for arbitrary use)?

### `Esp32PinMap`
The single source of truth translating this specific board's silkscreen labels
(D32, IO34, TOUCH9, VSPICLK, etc.) into `GpioPinId` + `PinCapabilities` pairs. This is
the one class that "knows" the diagram — everything else asks it questions rather
than hardcoding pin numbers or labels.

---

## Hardware Access Factory

### `Esp32Board`
An adapter factory / hardware-access facade — **not** the composition root itself.
The actual composition root is the project's `main.cpp`, which constructs
domain/application objects and wires them together; `Esp32Board` is one of the
things it asks for adapters from. Given a `PinMap`, exposes factory-style methods:

- `digitalOutput(pin): DigitalOutput`
- `digitalInput(pin): DigitalInput`
- `analogInput(pin): AnalogInput`
- `analogOutput(pin): AnalogOutput`
- `pwmOutput(pin): PwmOutput`
- `touchInput(pin): TouchInput`
- `i2cBus(bus: I2CBusId): I2CBus`
- `spiBus(bus: SpiBusId): SpiBus`
- `uartPort(port: UartPortId): UartPort`
- `clock(): Clock`

Internally validates each request against `PinCapabilities` (asking the pin what it
can do, not assuming) and throws a domain-specific exception —
`UnsupportedPinCapabilityException` — if, for example, touch input is requested on a
non-touch-capable pin. This keeps "ask, don't tell" honest at the one boundary where
hardware constraints are non-negotiable.

**On using an exception here:** this is a deliberate choice, not a default. C++
exceptions are fully supported on the ESP32 toolchain (unlike the Mega/AVR target
elsewhere in this project's history, which has no real libstdc++ and can't use them),
so nothing forces one error-signaling style over another. Exceptions were chosen
because these are construction-time/configuration errors — a pin genuinely doesn't
support what was requested — not runtime conditions calling code is expected to
handle and recover from. If that assumption stops holding (e.g. capability requests
start happening somewhere a caller needs to react to failure rather than treat it as
a startup-time bug), revisit in favor of a `Result`/status-code return instead.

### Global Constraints (not per-pin)

Some restrictions depend on board-wide state, not a single pin's own properties, so
`PinCapabilities` lookups alone can't catch them — `Esp32Board` needs to account for
these separately:

- **ADC2 vs. Wi-Fi.** ADC2 channels are unusable whenever the Wi-Fi radio is active.
  This board runs Wi-Fi continuously (MQTT to JMRI), so in practice ADC2 pins should
  probably be treated as unusable for `analogInput()` for the life of the program,
  not just conditionally — `Esp32Board` should refuse (or otherwise make loudly
  explicit) requests against them rather than let a read silently misbehave once
  Wi-Fi comes up.
- **PWM/LEDC channel pool.** 16 channels total across two timer groups, shared
  across every `pwmOutput()` request regardless of which pin it's for. Exhausting
  the pool should fail explicitly rather than silently double-assigning a channel.

---

## Testing-Support Classes (no mocking framework required)

| Fake | Purpose |
|---|---|
| `FakeDigitalOutput` | Records calls to `set()`, exposes last-known state for assertions |
| `FakeDigitalInput` | Returns a pre-programmed sequence/value for `read()` |
| `FakeAnalogInput` | Returns canned raw/voltage values |
| `FakeAnalogOutput` | Records written levels |
| `FakePwmOutput` | Records duty cycle writes |
| `FakeTouchInput` | Returns canned touch values/state |
| `RecordingI2CBus` | Stores writes for later assertion; returns canned reads |
| `RecordingSpiBus` | Stores transferred bytes for assertion |
| `RecordingUartPort` | Stores sent bytes; returns canned received bytes |
| `FakeClock` | Returns/advances a programmable "now" value, so timing-dependent logic (debounce, pulse timing, blink) is testable under `native` without real delays |

Because every capability interface is 1–2 methods, these fakes are trivial to write
by hand and stay well under the 8-line method budget — no `Mockito`/`MockK` needed
anywhere in this layer.

---

## Scope Note: Complete HAL vs. Needs-Driven

This class list covers everything the board exposes (`SpiBus`, `TouchInput`, a
second `I2CBus` instance, etc.), not just what the Tortoise turnout controller is
known to need — the turnout project may never exercise SPI or touch input at all.
That's a deliberate tradeoff, not an oversight: building the HAL as a complete,
reusable substrate for this board means more interfaces (and fakes) to write up
front, in exchange for not having to extend the HAL later if a future ESP32 project
on the same board needs them. If that trade isn't wanted, the leaner alternative is
to trim this list down to only `DigitalInput`, `DigitalOutput`, `PwmOutput`, and
`Clock` — everything the turnout controller is actually expected to use — and add
the rest only when a real need shows up.

---

## Explicitly Out of Scope

Domain/application classes such as `TouchSensorArray`, `ButtonDebouncer`,
`SensorHub`, or anything else that represents *what the board is being used for*
were deliberately left out. These depend entirely on the actual project (robot,
environmental sensor, IoT switch, etc.) and should be layered on top of this HAL
using the same TDD/DIP discipline once the project's purpose is known.
