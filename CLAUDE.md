# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Committing

Always use the `/arlo-commits` skill when committing changes in this repository — do not hand-write commit messages or `git commit` directly. Invoke it whenever the user asks for a commit, even if they don't name the skill explicitly.

## Project Purpose

Maltbee Turnout Controller — ESP32-WROOM-32 firmware that drives up to 8 Tortoise slow-motion switch machines per board, commanded via JMRI over MQTT. Domain/application design (Tortoise driver behavior, MQTT/JMRI adapters, turnout state model) is being developed separately and brought into this repo incrementally. See `docs/software-class-list.md` for the concrete value objects/ports/domain/adapter class breakdown (source of truth, synced from the design doc), and `docs/superpowers/specs/` / `docs/superpowers/plans/` for design and implementation history.

## Commands

```bash
pio test -e native                  # run host-native unit tests (no hardware needed)
pio run -e esp32dev                 # build the firmware for the ESP32-WROOM-32
pio run -e esp32dev --target upload
pio device monitor                  # serial monitor, 115200 baud
```

To run a single native test file:
```bash
pio test -e native -f test_<name>
```

There is no `native` build target for `src/` (`test_build_src = false` in `platformio.ini`) — native test binaries only compile `test/` plus whatever `lib/` code they include, not `main.cpp`.

**If `pio test -e native` fails to *run* (not compile) with a Windows status like `0xC0000139` / `STATUS_ENTRYPOINT_NOT_FOUND`:** this is a stale `libstdc++-6.dll` earlier on `PATH` shadowing the MinGW runtime the test binary was linked against, not a code problem. Make sure your MinGW `bin` directory is ahead of any other GCC/MinGW installs on `PATH` before running tests.

## Architecture

Hexagonal architecture, same discipline as the MaltbeeController project this was scaffolded from. The critical rule: **domain and application code must compile and run under the `native` PlatformIO environment without `Arduino.h`.** Hardware-specific code is isolated behind ports (interfaces) and only implemented in adapters.

- **Ports** (`lib/McsCore/src/ports/`) are pure interfaces the domain/application depend on.
- **Adapters** (`lib/McsCore/src/adapters/`) implement ports against real ESP32 hardware, guarded with `#ifdef ARDUINO` so they don't break the native build. Hand-written test doubles (`test/support/`) implement the same ports for native unit tests — no mocking framework.
- **`src/main.cpp` is the composition root only** — it wires adapters/domain/application objects together, calls setup once, and calls non-blocking `update()`/`poll()` methods from `loop()`. No business logic lives here.
- **No blocking calls** (`delay()`) in domain/application code — timing goes through the `Clock` port.
- Classes are built **needs-driven**, following the Build Order in `docs/software-class-list.md` — value objects first, then the smallest pure-logic domain class (`Debouncer`), working up to adapters and the composition root last.

### Current source layout

- `lib/McsCore/src/ports/` — port interfaces (`Clock`, `DigitalOutput`, `PwmOutput`; more added only as a real need arises)
- `lib/McsCore/src/domain/`, `lib/McsCore/src/application/`, `lib/McsCore/src/adapters/` — empty until real classes are needed
- `test/support/` — hand-written fakes (`FakeClock`, `FakeDigitalOutput`, `FakePwmOutput`, ...)
- `test/test_<name>/test_main.cpp` — Catch2 test binaries

**Why `native`'s `build_flags` includes `-Ilib/McsCore/src`:** PlatformIO's dependency finder only adds a `lib/` folder to the include path when some already-compiled file references it directly. A test file that only includes `support/FakeX.h` (which in turn includes `ports/X.h`) never triggers that discovery, since the fake isn't itself a recognized library dependency — so `ports/X.h` wouldn't resolve without the explicit `-I`. Keep this flag when adding new ports.

## Engineering Principles

- **TDD**: write a failing native test first, implement the minimum to pass, refactor only while green.
- **Dependency inversion**: domain depends on ports, adapters depend on domain-owned interfaces — never the reverse. No statics/singletons; everything is constructed and injected via the composition root.
- **Single responsibility, small interfaces**: 1–2 methods per port, one job per class.
- **Explicit state**: state changes go through methods that enforce valid transitions, not direct field mutation.
