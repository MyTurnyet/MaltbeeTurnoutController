# Maltbee Turnout Controller

Firmware for an ESP32-WROOM-32 board that drives up to 8 Tortoise slow-motion switch machines, commanded via JMRI over MQTT.

Built with Test-Driven Development and Hexagonal Architecture — domain and application logic is tested natively on a desktop, independent of the ESP32 hardware.

## Requirements

- PlatformIO
- ESP32-WROOM-32 dev board
- C++17 compiler (for native tests)

## Building and Testing

```bash
pio test -e native            # run native unit tests
pio run -e esp32dev           # build firmware
pio run -e esp32dev --target upload
pio device monitor
```

On Windows, if `pio test -e native` builds but the test binary fails to run, make sure your MinGW `bin` directory is ahead of any other GCC/MinGW installs on `PATH` — see `CLAUDE.md` for details.

## Status

Foundation hardware-abstraction-layer ports are in place, built needs-driven and TDD'd against the native test environment: `Clock`, `DigitalOutput`, `PwmOutput` (`lib/McsCore/src/ports/`), each with a hand-written fake (`test/support/`). No ESP32 hardware adapters, domain classes, or MQTT/JMRI communication yet — those come next as real needs arise. See `docs/esp32-hal-class-list.md` for the longer-term HAL reference and `docs/superpowers/` for design and implementation history.
