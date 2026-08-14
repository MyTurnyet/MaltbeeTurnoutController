# HAL Foundation & Scaffold Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stand up the PlatformIO/ESP32 project scaffold for Maltbee Turnout Controller and implement the first needs-driven hardware-abstraction-layer ports (`Clock`, `DigitalOutput`, `PwmOutput`) with hand-written fakes, fully TDD and native-testable.

**Architecture:** Hexagonal architecture (ports & adapters), matching `D:\Development\MaltbeeController`. Ports are pure abstract interfaces under `lib/McsCore/src/ports/`; hand-written fakes under `test/support/` stand in for hardware in native tests. No concrete ESP32 adapters or domain/application classes are built in this plan — those come later, once Tortoise-driving domain design is brought in from the user's other project.

**Tech Stack:** PlatformIO, Arduino framework (`espressif32` platform, `esp32dev` board), C++17, Catch2 3.7.1 (vendored, `test_framework = custom`).

## Global Constraints

- Domain and application code must compile and run under the `native` PlatformIO environment **without** `Arduino.h`. (`docs/superpowers/specs/2026-08-13-project-scaffold-design.md`)
- Only two PlatformIO environments exist: `esp32dev` and `native`. No `megaatmega2560` / Mega-specific code. (spec)
- `lib_deps = knolleary/PubSubClient@^2.8` on `esp32dev` only. (spec)
- No blocking calls (`delay()`) in domain/application code — timing goes through the `Clock` port. (engineering principles discussion)
- No mocking framework — every port interface stays small (1–2 methods) so a hand-written fake in `test/support/` satisfies it. (`docs/esp32-hal-class-list.md`)
- TDD throughout: failing native test first, minimal implementation, refactor only while green. (engineering principles discussion)
- Build HAL pieces in needs-driven order, not the full class list from `docs/esp32-hal-class-list.md` up front — only what's actually needed gets built. (user decision, this session)
- Every step in this plan that changes files ends with `pio run -e esp32dev` or `pio test -e native` passing before moving on — no unverified steps.

---

## Task 1: Scaffold the PlatformIO project

**Files:**
- Create: `platformio.ini`
- Create: `.gitignore`
- Create: `src/main.cpp`
- Create: `include/README`
- Create: `lib/README`
- Create: `test/README`
- Create: `lib/McsCore/src/domain/.gitkeep`
- Create: `lib/McsCore/src/ports/.gitkeep`
- Create: `lib/McsCore/src/adapters/.gitkeep`
- Create: `lib/McsCore/src/application/.gitkeep`
- Create: `test/support/.gitkeep`
- Create: `internal_documents/.gitkeep`
- Create: `lib/Catch2/` (vendored copy, from `D:\Development\MaltbeeController\lib\Catch2`)
- Create: `test/test_custom_runner.py` (copy, from `D:\Development\MaltbeeController\test\test_custom_runner.py`)
- Create: `.claude/skills/pioTest/SKILL.md` (copy, from `D:\Development\MaltbeeController\.claude\skills\pioTest\SKILL.md`)
- Create: `CLAUDE.md`
- Create: `README.md`

**Interfaces:**
- Consumes: nothing (first task).
- Produces: a building `esp32dev` environment and a `native` environment ready to run Catch2 tests (no test files yet — Task 2 adds the first one). Later tasks assume `platformio.ini`, `lib/Catch2/`, and `test/test_custom_runner.py` exist exactly as this task leaves them.

- [ ] **Step 1: Create `platformio.ini`**

```ini
[platformio]
default_envs = esp32dev

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_ldf_mode = deep+
lib_deps = knolleary/PubSubClient@^2.8

[env:native]
platform = native
test_framework = custom
test_build_src = false
build_flags = -std=c++17
```

- [ ] **Step 2: Create `.gitignore`**

```gitignore
# PlatformIO build output
.pio/

# PlatformIO generated project metadata
.pioenvs/
.piolibdeps/

# CLion / JetBrains project files
.idea/
cmake-build-*/

# CLion MCP server config (session-local port, not stable across machines/restarts)
.mcp.json

# CMake generated files
CMakeFiles/
CMakeCache.txt
cmake_install.cmake
Makefile

# Compiled binaries and object files
*.o
*.obj
*.a
*.lib
*.elf
*.hex
*.bin
*.exe
*.out

# Dependency files
*.d

# Debug files
*.map
*.lst

# Test output and coverage
coverage/
*.gcda
*.gcno
*.gcov

# Logs
*.log

# Operating system files
.DS_Store
Thumbs.db
desktop.ini

# Editor swap and temporary files
*.swp
*.swo
*~
*.tmp
*.temp

# Visual Studio Code settings, if used later
.vscode/

# Local environment files
.env
.env.*

# Python cache, if helper scripts are added later
__pycache__/
*.py[cod]

# Keep empty project directories if they contain .gitkeep files
!.gitkeep
```

- [ ] **Step 3: Create `src/main.cpp`**

```cpp
#include <Arduino.h>

void setup()
{
}

void loop()
{
}
```

- [ ] **Step 4: Create PlatformIO placeholder READMEs**

`include/README`:
```
This directory is intended for project header files.
```

`lib/README`:
```
This directory is intended for project specific (private) libraries.
PlatformIO will compile them to static libraries and link into the executable file.
```

`test/README`:
```
This directory is intended for PlatformIO Test Runner and project tests.
```

- [ ] **Step 5: Create empty hexagonal-layer folders**

Create these empty files (content is a single newline) so git tracks the otherwise-empty directories:
- `lib/McsCore/src/domain/.gitkeep`
- `lib/McsCore/src/ports/.gitkeep`
- `lib/McsCore/src/adapters/.gitkeep`
- `lib/McsCore/src/application/.gitkeep`
- `test/support/.gitkeep`
- `internal_documents/.gitkeep`

- [ ] **Step 6: Vendor Catch2 3.7.1**

Copy the entire vendored Catch2 library from the template project — this is third-party test-framework code, not something to hand-type:

```bash
cp -r "/d/Development/MaltbeeController/lib/Catch2" "/d/Development/MaltbeeTurnoutController/lib/Catch2"
```

Verify it copied completely:
```bash
diff -rq "/d/Development/MaltbeeController/lib/Catch2" "/d/Development/MaltbeeTurnoutController/lib/Catch2"
```
Expected: no output (directories identical).

- [ ] **Step 7: Copy the custom Catch2 test runner**

```bash
cp "/d/Development/MaltbeeController/test/test_custom_runner.py" "/d/Development/MaltbeeTurnoutController/test/test_custom_runner.py"
```

- [ ] **Step 8: Copy the `pioTest` Claude Code skill**

```bash
mkdir -p "/d/Development/MaltbeeTurnoutController/.claude/skills/pioTest"
cp "/d/Development/MaltbeeController/.claude/skills/pioTest/SKILL.md" "/d/Development/MaltbeeTurnoutController/.claude/skills/pioTest/SKILL.md"
```

- [ ] **Step 9: Write `CLAUDE.md`**

```markdown
# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Committing

Always use the `/arlo-commits` skill when committing changes in this repository — do not hand-write commit messages or `git commit` directly. Invoke it whenever the user asks for a commit, even if they don't name the skill explicitly.

## Project Purpose

Maltbee Turnout Controller — ESP32-WROOM-32 firmware that drives up to 8 Tortoise slow-motion switch machines per board, commanded via JMRI over MQTT. Domain/application design (Tortoise driver behavior, MQTT/JMRI adapters, turnout state model) is being developed separately and brought into this repo incrementally. See `docs/esp32-hal-class-list.md` for the planned hardware abstraction layer, and `docs/superpowers/specs/` / `docs/superpowers/plans/` for design and implementation history.

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

## Architecture

Hexagonal architecture, same discipline as the MaltbeeController project this was scaffolded from. The critical rule: **domain and application code must compile and run under the `native` PlatformIO environment without `Arduino.h`.** Hardware-specific code is isolated behind ports (interfaces) and only implemented in adapters.

- **Ports** (`lib/McsCore/src/ports/`) are pure interfaces the domain/application depend on.
- **Adapters** (`lib/McsCore/src/adapters/`) implement ports against real ESP32 hardware, guarded with `#ifdef ARDUINO` so they don't break the native build. Hand-written test doubles (`test/support/`) implement the same ports for native unit tests — no mocking framework.
- **`src/main.cpp` is the composition root only** — it wires adapters/domain/application objects together, calls setup once, and calls non-blocking `update()`/`poll()` methods from `loop()`. No business logic lives here.
- **No blocking calls** (`delay()`) in domain/application code — timing goes through the `Clock` port.
- Classes are built **needs-driven**, not speculatively — see `docs/esp32-hal-class-list.md`'s "Scope Note" section for why the full HAL class list exists as a reference but isn't implemented up front.

### Current source layout

- `lib/McsCore/src/ports/` — port interfaces (`Clock`, `DigitalOutput`, `PwmOutput`; more added only as a real need arises)
- `lib/McsCore/src/domain/`, `lib/McsCore/src/application/`, `lib/McsCore/src/adapters/` — empty until real classes are needed
- `test/support/` — hand-written fakes (`FakeClock`, `FakeDigitalOutput`, `FakePwmOutput`, ...)
- `test/test_<name>/test_main.cpp` — Catch2 test binaries

## Engineering Principles

- **TDD**: write a failing native test first, implement the minimum to pass, refactor only while green.
- **Dependency inversion**: domain depends on ports, adapters depend on domain-owned interfaces — never the reverse. No statics/singletons; everything is constructed and injected via the composition root.
- **Single responsibility, small interfaces**: 1–2 methods per port, one job per class.
- **Explicit state**: state changes go through methods that enforce valid transitions, not direct field mutation.
```

- [ ] **Step 10: Write `README.md`**

```markdown
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
```

- [ ] **Step 11: Verify the scaffold builds**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — the placeholder `main.cpp` compiles for the ESP32 target with no errors.

- [ ] **Step 12: Commit**

```bash
git add platformio.ini .gitignore src/main.cpp include/README lib/README test/README lib/McsCore lib/Catch2 test/support/.gitkeep test/test_custom_runner.py internal_documents/.gitkeep .claude/skills/pioTest/SKILL.md CLAUDE.md README.md
git commit -m "Scaffold PlatformIO project (esp32dev + native envs, empty hexagonal layers)"
```

---

## Task 2: `Clock` port + `FakeClock`

**Files:**
- Create: `lib/McsCore/src/ports/Clock.h`
- Create: `test/support/FakeClock.h`
- Test: `test/test_fake_clock/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `class Clock { virtual unsigned long nowMillis() const = 0; };` and `class FakeClock : public Clock` with `nowMillis() const`, `void setNow(unsigned long milliseconds)`, `void advanceBy(unsigned long milliseconds)`. Later tasks/domain code depend on this exact `Clock` interface for all timing.

- [ ] **Step 1: Write the failing test**

Create `test/test_fake_clock/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeClock.h"

TEST_CASE("FakeClock begins at zero")
{
    FakeClock clock;

    REQUIRE(clock.nowMillis() == 0);
}

TEST_CASE("setNow sets the reported time")
{
    FakeClock clock;

    clock.setNow(500);

    REQUIRE(clock.nowMillis() == 500);
}

TEST_CASE("advanceBy adds to the reported time")
{
    FakeClock clock;
    clock.setNow(100);

    clock.advanceBy(50);

    REQUIRE(clock.nowMillis() == 150);
}

TEST_CASE("advanceBy can be called multiple times")
{
    FakeClock clock;

    clock.advanceBy(10);
    clock.advanceBy(20);
    clock.advanceBy(30);

    REQUIRE(clock.nowMillis() == 60);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_fake_clock`
Expected: FAIL — compile error, `support/FakeClock.h` does not exist (`ports/Clock.h` doesn't exist either).

- [ ] **Step 3: Write the `Clock` port**

Create `lib/McsCore/src/ports/Clock.h`:

```cpp
#pragma once

class Clock
{
public:
    virtual ~Clock() = default;
    virtual unsigned long nowMillis() const = 0;
};
```

- [ ] **Step 4: Write `FakeClock`**

Create `test/support/FakeClock.h`:

```cpp
#pragma once

#include "ports/Clock.h"

class FakeClock : public Clock
{
public:
    unsigned long nowMillis() const override
    {
        return currentMillis_;
    }

    void setNow(unsigned long milliseconds)
    {
        currentMillis_ = milliseconds;
    }

    void advanceBy(unsigned long milliseconds)
    {
        currentMillis_ += milliseconds;
    }

private:
    unsigned long currentMillis_ = 0;
};
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_fake_clock`
Expected: PASS — 4 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — only `test_fake_clock` exists so far, 0 failures.

- [ ] **Step 7: Commit**

```bash
git add lib/McsCore/src/ports/Clock.h test/support/FakeClock.h test/test_fake_clock/test_main.cpp
git commit -m "Add Clock port and FakeClock test double"
```

---

## Task 3: `DigitalOutput` port + `FakeDigitalOutput`

**Files:**
- Create: `lib/McsCore/src/ports/DigitalOutput.h`
- Create: `test/support/FakeDigitalOutput.h`
- Test: `test/test_fake_digital_output/test_main.cpp`

**Interfaces:**
- Consumes: nothing (independent of `Clock`).
- Produces: `class DigitalOutput { virtual void set(bool state) = 0; };` and `class FakeDigitalOutput : public DigitalOutput` with `void set(bool state) override`, `bool isSet() const`, `int setCallCount() const`. Later Tortoise-driver adapters depend on this exact `DigitalOutput` interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_fake_digital_output/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeDigitalOutput.h"

TEST_CASE("FakeDigitalOutput begins low")
{
    FakeDigitalOutput output;

    REQUIRE_FALSE(output.isSet());
}

TEST_CASE("set(true) reports high")
{
    FakeDigitalOutput output;

    output.set(true);

    REQUIRE(output.isSet());
}

TEST_CASE("set(false) reports low")
{
    FakeDigitalOutput output;
    output.set(true);

    output.set(false);

    REQUIRE_FALSE(output.isSet());
}

TEST_CASE("set() records how many times it was called")
{
    FakeDigitalOutput output;

    output.set(true);
    output.set(false);
    output.set(true);

    REQUIRE(output.setCallCount() == 3);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_fake_digital_output`
Expected: FAIL — compile error, `support/FakeDigitalOutput.h` does not exist.

- [ ] **Step 3: Write the `DigitalOutput` port**

Create `lib/McsCore/src/ports/DigitalOutput.h`:

```cpp
#pragma once

class DigitalOutput
{
public:
    virtual ~DigitalOutput() = default;
    virtual void set(bool state) = 0;
};
```

- [ ] **Step 4: Write `FakeDigitalOutput`**

Create `test/support/FakeDigitalOutput.h`:

```cpp
#pragma once

#include "ports/DigitalOutput.h"

class FakeDigitalOutput : public DigitalOutput
{
public:
    void set(bool state) override
    {
        state_ = state;
        setCallCount_++;
    }

    bool isSet() const
    {
        return state_;
    }

    int setCallCount() const
    {
        return setCallCount_;
    }

private:
    bool state_ = false;
    int setCallCount_ = 0;
};
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_fake_digital_output`
Expected: PASS — 4 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — `test_fake_clock` and `test_fake_digital_output`, 0 failures.

- [ ] **Step 7: Commit**

```bash
git add lib/McsCore/src/ports/DigitalOutput.h test/support/FakeDigitalOutput.h test/test_fake_digital_output/test_main.cpp
git commit -m "Add DigitalOutput port and FakeDigitalOutput test double"
```

---

## Task 4: `PwmOutput` port + `FakePwmOutput`

**Files:**
- Create: `lib/McsCore/src/ports/PwmOutput.h`
- Create: `test/support/FakePwmOutput.h`
- Test: `test/test_fake_pwm_output/test_main.cpp`

**Interfaces:**
- Consumes: nothing (independent of `Clock` and `DigitalOutput`).
- Produces: `class PwmOutput { virtual void writeDutyCycle(double percent) = 0; };` and `class FakePwmOutput : public PwmOutput` with `void writeDutyCycle(double percent) override`, `double lastDutyCycle() const`, `int writeCallCount() const`. Later Tortoise-driver adapters depend on this exact `PwmOutput` interface.

- [ ] **Step 1: Write the failing test**

Create `test/test_fake_pwm_output/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakePwmOutput.h"

TEST_CASE("FakePwmOutput begins at zero duty cycle")
{
    FakePwmOutput output;

    REQUIRE(output.lastDutyCycle() == 0.0);
}

TEST_CASE("writeDutyCycle records the last value written")
{
    FakePwmOutput output;

    output.writeDutyCycle(42.5);

    REQUIRE(output.lastDutyCycle() == 42.5);
}

TEST_CASE("writeDutyCycle overwrites the previous value")
{
    FakePwmOutput output;

    output.writeDutyCycle(10.0);
    output.writeDutyCycle(90.0);

    REQUIRE(output.lastDutyCycle() == 90.0);
}

TEST_CASE("writeDutyCycle records how many times it was called")
{
    FakePwmOutput output;

    output.writeDutyCycle(1.0);
    output.writeDutyCycle(2.0);

    REQUIRE(output.writeCallCount() == 2);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_fake_pwm_output`
Expected: FAIL — compile error, `support/FakePwmOutput.h` does not exist.

- [ ] **Step 3: Write the `PwmOutput` port**

Create `lib/McsCore/src/ports/PwmOutput.h`:

```cpp
#pragma once

class PwmOutput
{
public:
    virtual ~PwmOutput() = default;
    virtual void writeDutyCycle(double percent) = 0;
};
```

- [ ] **Step 4: Write `FakePwmOutput`**

Create `test/support/FakePwmOutput.h`:

```cpp
#pragma once

#include "ports/PwmOutput.h"

class FakePwmOutput : public PwmOutput
{
public:
    void writeDutyCycle(double percent) override
    {
        lastDutyCycle_ = percent;
        writeCallCount_++;
    }

    double lastDutyCycle() const
    {
        return lastDutyCycle_;
    }

    int writeCallCount() const
    {
        return writeCallCount_;
    }

private:
    double lastDutyCycle_ = 0.0;
    int writeCallCount_ = 0;
};
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_fake_pwm_output`
Expected: PASS — 4 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: PASS — `test_fake_clock`, `test_fake_digital_output`, and `test_fake_pwm_output`, 0 failures.

- [ ] **Step 7: Commit**

```bash
git add lib/McsCore/src/ports/PwmOutput.h test/support/FakePwmOutput.h test/test_fake_pwm_output/test_main.cpp
git commit -m "Add PwmOutput port and FakePwmOutput test double"
```
