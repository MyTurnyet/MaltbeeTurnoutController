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
| `AnalogInput` | `readRaw(): Int`, `readVoltage(): Double` | ADC-capable pin (ADC0–ADC7, etc.) |
| `AnalogOutput` | `write(level: Double)` | DAC pins (GPIO25 / GPIO26) |
| `PwmOutput` | `writeDutyCycle(percent: Double)` | Any pin marked with the PWM squiggle |
| `TouchInput` | `readTouchValue(): Int`, `isTouched(): Boolean` | TOUCH0–TOUCH9 pins |
| `I2CBus` | `write(address, bytes)`, `read(address, length): ByteArray` | SDA/SCL pair (GPIO21/22) |
| `SpiBus` | `transfer(bytes): ByteArray` | Separate instances for HSPI and VSPI |
| `UartPort` | `send(bytes)`, `receive(): ByteArray` | TX0/RX0, TX2/RX2 |

**Notes:**
- `DigitalOutput`'s pin reference is immutable — only the on-hardware state changes,
  never the object's identity.
- `SpiBus` needs two separate instances/implementations because the board exposes
  both HSPI and VSPI buses independently.
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
ADC channel? DAC channel? Input-only (e.g. GPIO34–39, which have no output driver)?

### `Esp32PinMap`
The single source of truth translating this specific board's silkscreen labels
(D32, IO34, TOUCH9, VSPICLK, etc.) into `GpioPinId` + `PinCapabilities` pairs. This is
the one class that "knows" the diagram — everything else asks it questions rather
than hardcoding pin numbers or labels.

---

## Board-Level Composition Root

### `Esp32Board`
The composition root. Given a `PinMap`, exposes factory-style methods:

- `digitalOutput(pin): DigitalOutput`
- `digitalInput(pin): DigitalInput`
- `analogInput(pin): AnalogInput`
- `analogOutput(pin): AnalogOutput`
- `pwmOutput(pin): PwmOutput`
- `touchInput(pin): TouchInput`
- `i2cBus(): I2CBus`
- `spiBus(bus: SpiBusId): SpiBus`
- `uartPort(port: UartPortId): UartPort`

Internally validates each request against `PinCapabilities` (asking the pin what it
can do, not assuming) and throws a domain-specific exception —
`UnsupportedPinCapabilityException` — if, for example, touch input is requested on a
non-touch-capable pin. This keeps "ask, don't tell" honest at the one boundary where
hardware constraints are non-negotiable.

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

Because every capability interface is 1–2 methods, these fakes are trivial to write
by hand and stay well under the 8-line method budget — no `Mockito`/`MockK` needed
anywhere in this layer.

---

## Explicitly Out of Scope

Domain/application classes such as `TouchSensorArray`, `ButtonDebouncer`,
`SensorHub`, or anything else that represents *what the board is being used for*
were deliberately left out. These depend entirely on the actual project (robot,
environmental sensor, IoT switch, etc.) and should be layered on top of this HAL
using the same TDD/DIP discipline once the project's purpose is known.
