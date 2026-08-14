# Maltbee Turnout Controller — Project Scaffold Design

## Purpose

New standalone PlatformIO repository for ESP32-WROOM-32 firmware that drives
up to 8 Tortoise slow-motion switch machines per board, commanded via JMRI
over MQTT. This spec covers **scaffolding only** — the hexagonal-architecture
skeleton, build configuration, and native test harness. No turnout/Tortoise
domain logic is designed or implemented here; that design already exists in a
separate Claude project and will be brought in afterward.

## Relationship to MaltbeeController

Modeled on `D:\Development\MaltbeeController` (a Mega 2560 / LocoNet panel
project using hexagonal architecture, PlatformIO, and Catch2 native testing),
but is a fully separate repository with its own git history — not a fork, not
a shared codebase. Only structure and generic build tooling are carried over;
no Mega- or LocoNet-specific code, and no turnout/panel domain code.

## Environments (`platformio.ini`)

Two environments only:

- `[env:esp32dev]` — `platform = espressif32`, `board = esp32dev`,
  `framework = arduino`, `monitor_speed = 115200`, `lib_ldf_mode = deep+`,
  `lib_deps = knolleary/PubSubClient@^2.8` (MQTT client for JMRI
  communication, confirmed as the library to start with).
- `[env:native]` — `platform = native`, `test_framework = custom`,
  `test_build_src = false`, `build_flags = -std=c++17` (Catch2 native test
  harness, no hardware required).

No `megaatmega2560` environment — this project has no Mega/LocoNet hardware.
No per-target `src/` subfolder split (`src/mega/`, `src/esp32/`) — the
template needed that because one repo served two hardware targets; this repo
has only one, so `src/main.cpp` is the sole composition root.

## Folder layout

```
MaltbeeTurnoutController/
├── src/
│   └── main.cpp                      # placeholder composition root (empty setup()/loop())
├── lib/
│   ├── McsCore/src/
│   │   ├── domain/.gitkeep
│   │   ├── ports/.gitkeep
│   │   ├── adapters/.gitkeep
│   │   └── application/.gitkeep
│   ├── Catch2/                       # vendored Catch2 3.7.1, copied as-is from template
│   └── README                        # PlatformIO standard placeholder
├── test/
│   ├── support/.gitkeep
│   ├── test_example/test_main.cpp    # one passing Catch2 test proving the harness works
│   ├── test_custom_runner.py         # copied as-is (Catch2 output parser for PlatformIO)
│   └── README
├── include/
│   └── README
├── docs/                             # empty aside from this spec, ready for docs brought over later
├── internal_documents/.gitkeep
├── .claude/skills/pioTest/SKILL.md   # copied as-is
├── .gitignore                        # copied as-is from template
├── platformio.ini
├── CLAUDE.md                         # rewritten for this project's purpose
└── README.md                         # new, short project description
```

Not carried over:
- `.mcp.json` — gitignored in the template too (session-local CLion port);
  regenerate locally if/when needed.
- `.claude/settings.local.json`, `.claude/scheduled_tasks.lock` — machine-local
  runtime state, not template content.
- `.idea/`, `.pio/` — IDE workspace and build output, regenerated locally.

## `lib/McsCore` empty layers

`domain/`, `ports/`, `adapters/`, `application/` are created empty
(`.gitkeep` only) so the hexagonal boundaries exist as a directory structure
from day one, but no classes are pre-written — domain design (Tortoise
driver ports, MQTT/JMRI adapters, turnout domain model) will be brought in
from the user's other Claude project.

## Native test harness

Vendored Catch2 3.7.1 (`lib/Catch2/`) and the custom PlatformIO test runner
(`test/test_custom_runner.py`) are copied unchanged from the template — this
is build tooling, not domain code, and the `test_framework = custom` env
depends on both being present. `test/test_example/test_main.cpp` contains one
trivial `TEST_CASE` (e.g. asserting `1 + 1 == 2` or similar) so
`pio test -e native` has something to run and pass immediately, proving the
harness works before any real code is written.

## CLAUDE.md

Rewritten (not copied) to describe this project's actual purpose (ESP32 +
Tortoise + JMRI/MQTT), reference the two environments and their commands
(`pio test -e native`, `pio run -e esp32dev`, `pio run -e esp32dev --target
upload`, `pio device monitor`), restate the hexagonal-architecture ground
rules (domain/application must compile under `native` without `Arduino.h`),
and keep the instruction to commit via `/arlo-commits`.

## Git

`git init` a fresh repository (no shared history with MaltbeeController), one
initial commit containing the full scaffold once created.

## Out of scope

- Any Tortoise driver, MQTT adapter, or turnout domain class — brought in
  later from the user's other Claude project.
- Board-specific tuning beyond the generic `esp32dev` PlatformIO board
  definition (user confirmed this is correct for their hardware).
- CI configuration, PCB/wiring docs, roadmap/milestone documents — not
  requested.
