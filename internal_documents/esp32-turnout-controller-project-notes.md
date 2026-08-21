# ESP32 Turnout Controller — Project Notes

Working document. Last updated: 21 August 2026.

**Rev 2 — supersedes `esp32-turnout-controller-notes.md`.** Changes in this revision:

- **Corrected Tortoise pin roles in §3.5 and §6.** The previous revision had motor on pins 4/5 and contact set 2 as 6-7-8. Both were wrong. Motor is pins 1 and 8; contact set 2 is 5-6-7 with common on pin 5. Verified with a continuity tester on the breadboard build. The breadboard prototype doc was already correct.
- **Struck the frog polarity exception in §3.7.** Frog polarity is no longer routed through this project at all.
- **Open item 2 closed** — screw terminals.
- **BOM corrections** — 470 µF quantity 1 → 4; 10 kΩ quantity 4 → 8; added LED, resistor, terminal blocks, standoffs.
- **§4 pin assignment** now includes STBY on GPIO 2.

---

## 1. Project goal

A WiFi-connected turnout controller for an N-scale layout, built around an ESP32, driving existing Tortoise stall motors, integrated with JMRI.

## 2. Layout and hardware context

| Item | Value |
|---|---|
| Scale | N |
| Existing turnout machines | Circuitron Tortoise (stall motor) |
| Controller | ESP32-WROOM-32, 30-pin USB-C dev board |
| Control system | DCC, with JMRI |
| Track voltage | Reads 15.6 V on a standard DMM — **measurement not trustworthy, see open item 1** |
| Turnouts per node | 8 |

---

## 3. Decisions made

### 3.1 Actuator: keep the Tortoises

Considered switching to servos (MG90S class). Decided against.

**Reasoning:** the Tortoise's two built-in SPDT contact sets are doing real work — one is available for frog polarity, the other gives genuine position feedback. Servos have neither, so each would need a microswitch, a mount, and a compliant linkage, which erodes most of the unit-cost advantage. Servos also bring buzz/hunting (needs PWM detach on arrival), power-on twitch that can stress point rails, per-turnout endpoint calibration, and plastic gears in a location that's painful to service. Tortoises draw ~15 mA and last decades.

**Not permanently closed.** The software design keeps an actuator abstraction, so servos remain reversible. If evaluated, do it on 2–3 turnouts on a test plank first.

### 3.2 Node size: 8 turnouts per ESP32

Technical ceiling is far higher (I²C expanders scale to 128+ outputs), but the real constraints are physical: wire run length from node to turnout, the ~1 m practical limit on an I²C bus, blast radius when a node fails mid-session, and under-layout serviceability.

**Principle: size the node to a layout region, not to a chip's pin count.** If a yard throat has 11 turnouts, that's the node. WiFi means extra nodes cost no bus wiring.

At 8 turnouts, no I/O expander is needed at all.

### 3.3 Driver: TB6612FNG dual H-bridge × 4

**Alternatives rejected:**

- *Discrete transistor H-bridge.* Electrically viable at 15 mA, but the high-side PNPs can't be turned off by a 3.3 V GPIO, so each needs its own level-shifting transistor. That's 6 transistors + ~8 resistors per channel — 112 parts and 200+ jumpers for eight channels.
- *DPDT relays.* Only 4 parts per channel, but a 12 V coil draws 30–40 mA, more than double the Tortoise itself, and adds a wearing mechanical part to a system whose main virtue is that it doesn't wear.
- *L293D.* Viable DIP alternative if TB6612 breakouts prove hard to source.

### 3.4 Pin economy: 74HC04 hex inverter × 2

A Tortoise's state is strictly binary — coast and brake are never wanted. So IN2 is always the inverse of IN1, and feeding it from an inverter halves the GPIO cost to **one pin per turnout**.

Two hex inverters give 12 channels; 8 used, 4 spare (useful headroom to 12 turnouts before an MCP23017 would be needed). **Unused inverter inputs must be tied to ground** — floating CMOS inputs oscillate and draw supply current.

### 3.5 Feedback: use Tortoise contact set 2

Reports *achieved* position rather than *commanded* position. This is a real advantage over DCC accessory decoders, which have no feedback path at all.

Contacts are dry and isolated: **common is pin 5**, to ground. **Pin 6** goes to the GPIO with a pull-up. Pin 7 is the other throw terminal and stays unused — that's the v2.0 two-sensor terminal. No level shifting needed.

**Two-sensor feedback is out of scope for the first board.** It would need a second GPIO per channel and an extra terminal pole per channel, and it belongs in a second revision if it's wanted at all. The `FeedbackSensor` interface accommodates it later without disturbing the class design.

### 3.6 JMRI integration: MQTT

JMRI has native MQTT-connected turnouts (since release 4.26), plus sensors, lights and signal masts. Payload is `CLOSED` / `THROWN`. A `{0}` placeholder allows the suffix mid-topic, e.g. `track/turnout/{0}/state`. A separate receive topic carries state back, which is how the position feedback reaches JMRI.

**Confirmed topic scheme:** `track/turnout/` — no leading slash, no `/trains/` prefix. Earlier drafts of the broker/JMRI companion doc assumed `/trains/track/turnout/`; that was wrong and was corrected against the live JMRI installation.

**LCC / OpenLCB considered and deferred.** OpenMRNLite supports ESP32 over WiFi or CAN, and self-describing CDI would let JMRI configure nodes directly. But OpenMRN is a large framework with its own execution model that would dominate the architecture rather than sit behind an interface. Revisit later — the abstraction makes it a swap of two implementations, not a rewrite.

Requires an MQTT broker (Mosquitto).

### 3.7 Power: dedicated 12 V DC accessory bus

**Explicitly not** rectified DCC track power. Reasons:

- Track power drops on every short — derailment, wrong-set turnout, dropped tool. Each drop reboots the controller: WiFi reassociation, MQTT reconnect, JMRI re-establishment. Seconds of dead turnouts.
- **Circular failure mode:** wrong turnout → derailment → short → booster trips → controller dies → can't throw the turnout that caused it.
- ~400 mA competes with the locomotive budget on the booster.
- Switching noise couples both ways; DCC packet corruption is a real risk.
- Grounding gets awkward with multiple power districts.

Accessory decoders get away with track power because they draw tens of milliamps, hold no state, and recover in milliseconds. A WiFi node with a JMRI session doesn't.

**No track-bus connection at all.** An earlier revision noted frog polarity as an exception, coming off the track bus through Tortoise contact set 1. That exception no longer applies — frog polarity is out of scope for this project. Nothing on this board touches track power, and contact set 1 goes unused. This is a genuine simplification: no locomotive-level current anywhere near the controller, and 24 fewer terminal poles.

A dedicated bus also scales — future nodes tap it locally instead of each needing its own supply.

### 3.8 Construction: fabricated PCB

Breadboard is fine for prototyping. For permanent under-layout installation, spring contacts oxidise and lose grip under vibration and humidity, producing intermittent faults that masquerade as software bugs.

**Decided: fabricated PCB.** Layout decisions are recorded in the companion doc `pcb-design-decisions.md`.

Consequences:

- **Socket everything.** Female headers for both the TB6612 modules and the ESP32 dev board. Dev boards fail, get swapped, or get pulled for bench reflashing — a soldered-down ESP32 turns any of those into a desoldering job while lying on your back.
- **Measure the actual parts before laying out footprints.** Screw terminal pitch comes in 3.5 / 3.81 / 5.08 mm; ESP32 dev board row spacing varies between manufacturers. Designing against a guessed footprint and finding out when the boards arrive costs weeks. See open item 3a.
- **Do not send to fabrication until the prototype channel works.** A PCB freezes mistakes into fiberglass.
- Bare SSOP-24 TB6612FNG chips instead of modules would be cheaper and more compact, but adds 0.65 mm pitch soldering and the support passives the modules provide. Consider for a second revision, not the first board.

### 3.9 Language: C++ with PlatformIO

**Decided.** An existing C++/PlatformIO project already in flight was the deciding factor.

**The criterion that mattered:** TDD requires the domain suite to compile and run on the development machine in under a second. If tests only run by flashing hardware, the feedback loop is 30+ seconds and TDD collapses. PlatformIO's `native` build environment compiles the hardware-free sources with the host compiler, giving a desktop test binary that runs in milliseconds.

**How the principles map:**

| Principle | C++ mechanism |
|---|---|
| Interfaces | Pure virtual base classes |
| Dependency inversion | Constructor injection of interface references |
| Immutability | `const` value objects, no setters |
| No mocking framework | Hand-written classes implementing the same pure virtual base |
| Composition over inheritance | Inheritance used only to implement interfaces |

**Test framework:** Unity ships with PlatformIO; doctest or Catch2 also work.

**Documented exception to "no statics":** Arduino's `setup()`/`loop()` structure has nowhere to put the object graph except file scope. Use exactly one file-scope composition root object, with everything else constructed inside it and reached only through it. One static as an entry point is categorically different from statics scattered through the domain.

**No dynamic allocation after boot.** The whole object graph is constructed once at startup and never changes.

**Considered and rejected:**

- *Rust (esp-rs)* — arguably a better philosophical fit: traits are cleaner than pure virtuals, immutability is the default, and `static mut` requires `unsafe`. Rejected on ecosystem depth and toolchain friction (classic ESP32 is Xtensa, needing Espressif's fork via `espup`), plus ownership friction with shared collaborators (`Rc<RefCell<T>>`).
- *MicroPython* — domain tests would run under CPython while production runs MicroPython; not the same runtime. No enforcement of immutability or no-statics.
- *C* — hand-built vtables from function pointers; fights every principle on the list.
- *TinyGo* — no ESP32 WiFi support.

The domain core has no hardware dependencies, so a later port would be mechanical rather than a rewrite.

---

## 4. Pin assignment

| Channel | Control out | Feedback in |
|---|---|---|
| 1 | GPIO 13 | GPIO 36 |
| 2 | GPIO 14 | GPIO 39 |
| 3 | GPIO 27 | GPIO 34 |
| 4 | GPIO 26 | GPIO 35 |
| 5 | GPIO 25 | GPIO 16 |
| 6 | GPIO 33 | GPIO 17 |
| 7 | GPIO 32 | GPIO 18 |
| 8 | GPIO 4 | GPIO 19 |

- Status LED: GPIO 23
- **TB6612 `STBY`: GPIO 2**, with a 10 kΩ pull-down to ground. See below.
- Reserved for future I²C: GPIO 21 (SDA), GPIO 22 (SCL)
- **Nothing on GPIO 12** — held high at reset it selects the wrong flash voltage and the board may not boot.
- GPIO 34 / 35 / 36 / 39 are input-only and have **no internal pull-ups**. On the PCB, all eight channels get an external 10 kΩ regardless — see §5 note.

### Why STBY is a GPIO rather than a hard tie

Earlier drafts tied `STBY` permanently high. That works electrically but produces a bad power-on behaviour: ESP32 GPIOs leave reset high-impedance, so every `xIN1` floats, every inverter output is indeterminate, and all four bridges drive to arbitrary polarity. Every boot would randomly throw turnouts.

Driving `STBY` from GPIO 2 with a pull-down means the bridges are disabled until firmware asserts them. GPIO 2 is a strapping pin, but it wants low or floating at boot anyway, so the pull-down is the safe direction. On most 30-pin boards it also drives the onboard LED, which is harmless. A solder jumper to 3.3 V is the fallback if GPIO 2 misbehaves.

This is also a partial answer to open item 8: nothing moves until commanded, and the node can read actual positions from feedback before deciding whether to move anything.

### 30-pin board GPIO reality (for reference)

Of 25 broken-out GPIOs: 4 are input-only (34/35/36/39), 2 are USB serial (1/3), 4 are fussy strapping pins (2/5/12/15). That leaves 15 comfortable outputs.

---

## 5. Bill of materials

### Ordered (all placed)

| Part | Qty | Notes |
|---|---|---|
| TB6612FNG dual H-bridge breakout | 4 | Headers ship loose, need soldering. VM max 15 V. |
| SN74HC04N hex inverter, DIP-14 | 2 | TI-branded. Runs at 3.3 V. |
| LM2596 adjustable buck converter | 1 | **Set output to 5 V and verify before connecting anything.** Ships at arbitrary voltage. |
| 100 nF ceramic capacitor | 6 | One per IC, across supply pins. |
| 12 V 2 A DC supply | 1 | Total load ~400 mA; headroom for a second node. |
| Perfboard / screw terminal / header kit | 1 | Terminals and female headers for the PCB. |
| DIP IC socket assortment | 1 | Need 2× DIP-14 for the inverters. |

### Corrected quantities

| Part | Qty | Was | Why |
|---|---|---|---|
| 470 µF electrolytic, ≥25 V | **4** | 1 | One per TB6612 on the 12 V rail. Four modules, four caps. |
| 10 kΩ resistor | **8** | 4 | All eight channels get an external pull-up, not just the four input-only pins. Parallel with an internal pull-up gives ~8 kΩ, which a dry contact doesn't care about. Eight identical channel instances beats a four-and-four asymmetry that can be misassembled. |

### Added for the PCB

| Part | Qty | Notes |
|---|---|---|
| 5 mm red LED | 1 | Status indicator, board-mounted at an edge. Standard 2.54 mm lead spacing. |
| 330 Ω resistor | 1 | LED series resistor. ~4 mA at 3.3 V. |
| 4-pole screw terminal, 5.08 mm | 8 | One per channel: motor +, motor −, feedback, feedback common. |
| 2-pole screw terminal, 5.08 mm | 1 | 12 V bus in. |
| M3 nylon standoffs, 6–10 mm | 4 | Nylon rather than metal — keeps the board electrically isolated from the benchwork. |
| Schottky diode or P-FET | 1 | Reverse polarity protection on the 12 V input. |
| 1 A fuse + holder | 1 | 12 V input. |

### Already on hand

- ESP32-WROOM-32 30-pin dev board

### Still to source

- **Bus wire.** 18 AWG under ~25 ft; 16 AWG beyond. Size for the eventual multi-node total, not this node's 400 mA.
- **Tortoise harness wire.** 22–24 AWG is ample — motor is ~15 mA and feedback is a dry contact.
- ~~Enclosure~~ — **not planned.** Board mounts on standoffs, open.

---

## 6. Wiring summary

**Power:** 12 V bus → buck converter → 5 V → ESP32 VIN. 12 V also feeds VM on each TB6612 directly. Common ground throughout.

**Per TB6612:** VM = 12 V, VCC = 3.3 V, PWMA and PWMB tied high (no speed control wanted), STBY driven from GPIO 2.

**Per channel:** GPIO → AIN1 direct, and → 74HC04 → AIN2.

**Tortoise connections — corrected.** The previous revision of this table was wrong in two places and would have produced a miswired harness.

| Tortoise pin | Goes to | Note |
|---|---|---|
| 1 | Bridge output `xO1` | Motor winding |
| 8 | Bridge output `xO2` | Motor winding |
| 5 | Ground | Common of contact set 2 |
| 6 | Feedback GPIO, and through 10 kΩ to 3.3 V | One throw of contact set 2 |
| 7 | unused | Other throw — the v2.0 two-sensor terminal |
| 2, 3, 4 | unused | Contact set 1. Frog polarity is out of scope; see §3.7. |

Motor is pins **1 and 8**, not 4 and 5. Pins 4 and 5 are the contact commons — 4 for set 1, 5 for set 2. Confirmed by continuity test on the breadboard build: pins 1 and 8 read a few hundred ohms through the winding, pin 4 flips between 2 and 3, pin 5 flips between 6 and 7.

**Decoupling:** 100 nF across every IC's supply pins; 470 µF on the 12 V rail, one per TB6612.

---

## 7. Software architecture (agreed direction)

Language and toolchain decision in §3.9. C++ with PlatformIO. Full class breakdown in the companion software class list doc.

Hardware-free domain core, compiled for the host via PlatformIO's `native` environment. Only driver-layer files include Arduino headers — this separation is what makes the fast test loop possible, so it's a hard rule rather than a preference.

**Three layers:**

1. *Domain* — no Arduino headers, no `delay()`, no hardware types. Compiles on the host.
2. *Drivers* — implement the domain's interfaces against real hardware. Thin, minimal logic.
3. *Composition root* — one file-scope object wiring layers 1 and 2 together in `setup()`.

**Interfaces (pure virtual):**

- `TurnoutActuator` — throw and report; implementations `StallMotorActuator`, later possibly `ServoActuator`
- `Clock` — time arrives through here, never `delay()`, so timing logic is host-testable
- `CalibrationStore` — NVS-backed persistence behind an interface
- `TurnoutCommandSink` — JMRI tells us
- `PositionReporter` — we tell JMRI

**Value objects:** `TurnoutId`, `TurnoutPosition`, `TurnoutState`

**Testing:** hand-written fakes implementing the interfaces. No mocking framework — if one were needed, the boundaries would be wrong.

MQTT never appears in the domain. `MqttPositionReporter` is the only file that knows a broker exists; swapping to LCC later means writing `LccPositionReporter`, not touching the core.

**Firmware consequence of the pull-up decision:** all eight feedback pins use plain `INPUT`, never `INPUT_PULLUP`, since every channel has an external resistor. `EspDigitalInput`'s pull-up flag is therefore the same for all eight.

**Firmware consequence of the STBY decision:** the composition root must drive GPIO 2 high in `begin()` before any turnout can move, and should leave it low until the object graph is built.

---

## 8. Open items — need discussion or decision

| # | Item | Notes |
|---|---|---|
| 1 | **Verify track voltage properly** | DMMs are average-responding and calibrated for 60 Hz sine; DCC is a ~5–9 kHz square wave, so the 15.6 V reading is an artifact. Use an RRampMeter or scope. If genuinely near 15.6 V, that's high for N — check the command station scale setting. Lower priority now that nothing on this board touches track power. |
| 2 | ~~Wiring harness style~~ | **Closed — screw terminals.** 5.08 mm pitch, 4-pole blocks, one per channel. See `pcb-design-decisions.md`. |
| 3 | ~~Construction method~~ | **Closed — fabricated PCB.** See §3.8. |
| 3a | **Measure terminal and header footprints** | **In progress — prerequisite for layout.** Checklist in `pcb-design-decisions.md`. |
| 3b | PCB fab house and design tool | KiCad. Nothing on this board is fine-pitch and the 12 V rail carries ~400 mA, so default fab rules at any house are more than adequate — choose on cost and lead time, not capability. |
| 4 | Object model + host test setup | Class list complete. Build order defined. |
| 5 | ~~MQTT broker placement~~ | **Closed — Mosquitto on the JMRI Surface.** Running as a Windows service. |
| 6 | ~~Topic scheme and system-name allocation~~ | **Closed.** Node-prefixed, Node N owns N01–N16. Topic base `track/turnout/`. Node ID 1 set and permanent. |
| 7 | JMRI feedback mode | Direct vs. Monitoring. Test empirically — see the broker/JMRI doc §5.5. |
| 8 | Power-on behaviour | Partially answered by the STBY decision (§4): nothing moves until commanded. Still open whether the node should actively drive turnouts to a known state after reading feedback, or leave them. |
| 9 | Local web UI | For diagnostics and manual throw while standing at the layout. |
| 10 | OTA firmware updates | Worth having before the node is mounted under the layout. |
| 11 | ~~Enclosure and mounting~~ | **Closed — no enclosure.** Board mounts on 4× M3 nylon standoffs. |
| 12 | Multi-node addressing scheme | Decide before building the second node, not after. |
| 13 | Panel pushbuttons | Deferred — JMRI-only control chosen for now. Note that MQTT means buttons need the broker up; LCC would allow peer-to-peer operation without JMRI running. |
| 14 | **Status LED blink patterns** | New. What the LED signals, and whether it gets a `StatusIndicator` domain class with `ManualClock`-testable timing. Doesn't block the board. |
| 15 | **Mac `mosquitto_pub` "Bad file descriptor"** | New. Unresolved. Doesn't block anything — the Windows side works. |

---

## 9. Suggested build sequence

1. **~~Domain core plus host tests~~** — interfaces, value objects, hand-written fakes.
2. **~~Stand up Mosquitto and prove the JMRI side~~** — broker running as a service, firewall open, topic scheme confirmed.
3. **Prototype one channel** — breadboard build complete through §7 (both Tortoises swinging). §8/§9 feedback verification still outstanding.
4. **MQTT integration on hardware** — one turnout end-to-end from the JMRI turnout table, including feedback reporting.
5. **PCB layout and fabrication** — **current step.** Blocked on 3a measurements and on step 3's feedback verification.
6. **Scale to 8 channels** on the fabricated board.
7. **Install and burn in** — run a session before trusting it.
