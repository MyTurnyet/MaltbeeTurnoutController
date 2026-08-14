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

## Status

Scaffolding in progress. Domain design (Tortoise driving, MQTT/JMRI communication) is being developed separately and will be incorporated incrementally — see `docs/esp32-hal-class-list.md` and `docs/superpowers/`.
