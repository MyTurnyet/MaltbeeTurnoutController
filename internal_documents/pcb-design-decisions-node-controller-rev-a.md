# PCB Design Decisions — Node Controller Rev A

Companion to the project notes (rev 2) and the breadboard prototype doc. Covers build-sequence step 5: KiCad layout and fabrication.

Created 21 August 2026, on completion of the breadboard prototype through §7.

---

## 0. Status

**Not yet started in KiCad.** Two things gate the schematic:

1. **Breadboard §8/§9 feedback verification.** Until `fb1` and `fb2` track `cmd` reliably in the serial monitor, two of the five claims in the prototype doc's §10 are unproven — including whether the input-only pins behave with external pull-ups, which is eight resistor footprints on this board.
2. **Open item 3a footprint measurements** (§6 below).

Everything else on this page is decided.

---

## 1. Scope

One node: 8 turnout channels, WiFi/MQTT, position feedback. Socketed ESP32 and TB6612 modules. No enclosure.

**Explicitly out of scope for Rev A:**

- **Frog polarity.** Not routed through this board at all. Wire it at the machine off contact set 1 if it's ever wanted. Keeping locomotive-level DCC current off a logic board is worth more than the convenience, and it saves 24 terminal poles.
- **Two-sensor feedback.** Needs a second GPIO and a terminal pole per channel. A first board shouldn't be padded with speculation — that's what Rev B is for.
- **Fascia indication.** Contact set 1 could drive a bicolour LED passively, with no controller involvement. Wire it at the machine if wanted.

---

## 2. Connector edge

**Screw terminals, 5.08 mm pitch, horizontal wire entry.**

Pitch reasoning: 32 poles at 5.08 mm is 163 mm of edge, which splits across two edges as ~81 mm each and stays inside a 100 × 100 mm outline — where the low-cost fab price breaks sit. 3.5 mm would fit more easily but is genuinely unpleasant to drive while lying on your back under benchwork, which is exactly the position 32 terminations will be made in.

### Pole budget

| Block | Poles | Contents |
|---|---|---|
| 8 × 4-pole (one per channel) | 32 | Motor +, motor −, feedback, feedback common |
| 1 × 2-pole | 2 | 12 V bus in |
| **Total** | **34** | |

Four-pole blocks, one per channel, rather than ganged two-pole. Silkscreen reads `CH1  M1  M8  FB  COM` and a miswire is visible at a glance.

### Terminal pin mapping

| Pole | Tortoise pin | Note |
|---|---|---|
| M1 | 1 | Motor winding |
| M8 | 8 | Motor winding |
| FB | 6 | Contact set 2, one throw |
| COM | 5 | Contact set 2 common — to ground |

A per-channel ground pole rather than daisy-chaining commons under the layout. Costs 8 poles, saves a class of intermittent fault.

**Wire gauge:** 22–24 AWG for the machine harness (motor is ~15 mA, feedback is a dry contact). 18 AWG for the 12 V bus in.

---

## 3. Schematic structure — hierarchical

**Root sheet:** ESP32, LM2596, 4 × TB6612, 2 × 74HC04, all decoupling, 12 V terminal block, status LED, eight instances of the channel sheet.

**Channel sheet** (`channel.kicad_sch`, instantiated 8×): 4-pole screw terminal, 10 kΩ pull-up, and the nets between hierarchical pins.

| Hierarchical pin | Type |
|---|---|
| `CTRL_DIRECT` | input |
| `CTRL_INV` | input |
| `FB` | output |

`+3V3` and `GND` as global power symbols rather than hierarchical pins — less clutter across eight instances.

### Why the ICs stay on the root sheet

The obvious factoring — one sheet owning "everything for one turnout" — doesn't survive contact with the parts. A TB6612 is a *dual* bridge and a 74HC04 is a *hex* inverter, so neither maps to one channel. Put a TB6612 inside the channel sheet and eight instances demand eight modules; the design has four.

So the channel sheet owns only what is genuinely per-channel, and the shared plumbing lives above it. This mirrors the software, where `Turnout` is deliberately thin and the registry owns the fan-out.

### Channel allocation

| Ch | GPIO out | FB in | TB6612 | Bridge | 74HC04 | Inverter (in→out) |
|---|---|---|---|---|---|---|
| 1 | 13 | 36 | U1 | A | U5 | 1 → 2 |
| 2 | 14 | 39 | U1 | B | U5 | 3 → 4 |
| 3 | 27 | 34 | U2 | A | U5 | 5 → 6 |
| 4 | 26 | 35 | U2 | B | U5 | 9 → 8 |
| 5 | 25 | 16 | U3 | A | U5 | 11 → 10 |
| 6 | 33 | 17 | U3 | B | U5 | 13 → 12 |
| 7 | 32 | 18 | U4 | A | U6 | 1 → 2 |
| 8 | 4 | 19 | U4 | B | U6 | 3 → 4 |

U5 fills all six inverters; U6 uses two, four spare — matching the headroom-to-12-turnouts note in project notes §3.4.

Each `CHn_D` net fans out to two loads: the TB6612 input and the corresponding inverter input.

### Fixed ties

| Signal | Tie to |
|---|---|
| `PWMA`, `PWMB` (all 4 modules) | `+3V3` |
| `STBY` (all 4 modules) | `STBY` net → GPIO 2 |
| GPIO 2 | 10 kΩ pull-down to `GND` |
| U6 pins 5, 9, 11, 13 (unused inputs) | `GND` |
| U6 pins 6, 8, 10, 12 (unused outputs) | no connect |

**Ground the unused inverter inputs.** Floating CMOS inputs oscillate and draw supply current.

---

## 4. Power

| Net | Source | Loads |
|---|---|---|
| `+12V` | J1-1, via fuse and reverse-polarity device | LM2596 `IN+`, 4 × TB6612 `VM`, 4 × 470 µF |
| `+5V` | LM2596 `OUT+` | ESP32 `VIN` |
| `+3V3` | ESP32 `3V3` | 2 × 74HC04 pin 14, 4 × TB6612 `VCC`, 8 × pull-ups, PWMA/PWMB |
| `GND` | J1-2 | everything |

**Reverse polarity protection and a 1 A fuse on the 12 V input.** Backwards 12 V into four `VM` pins is an expensive mistake to make once, and with no enclosure the terminal block is exposed under the layout next to a track bus.

### Decoupling

| Part | Qty | Placement |
|---|---|---|
| 100 nF | 2 | 74HC04 pins 14–7, as close as the footprint allows |
| 100 nF | 4 | TB6612 `VCC`–`GND`, one per module |
| 470 µF, ≥25 V | 4 | `+12V` to `GND`, one adjacent to each TB6612 |

The BOM was sized for a single-module prototype and lists one 470 µF. Four are needed.

### LM2596

On female headers, per the socket-everything rule. **Check pot accessibility with the module seated** — it will need re-trimming in place.

---

## 5. Decided details

### Pull-ups — fit all eight

Channels 1–4 use GPIO 36/39/34/35, which are input-only with no internal pull-up. Channels 5–8 could use `INPUT_PULLUP`. That asymmetry breaks the "eight identical instances" story.

**Fit an external 10 kΩ on all eight.** An external pull-up on a pin that also has an internal one is harmless — the parallel combination is ~8 kΩ instead of 10 kΩ, and a Tortoise dry contact doesn't care. One sheet, eight identical instances, four extra resistors costing pennies, and no asymmetry to get wrong at assembly.

Firmware consequence: plain `INPUT` on all eight, never `INPUT_PULLUP`. `EspDigitalInput`'s pull-up flag is the same for every channel.

The alternative — a `HAS_PULLUP` DNP variant per instance — buys nothing and adds a BOM line that can be misread.

### STBY on GPIO 2

See project notes §4. Bridges stay disabled until firmware asserts them, so boot doesn't randomly throw turnouts. Solder jumper to 3.3 V as fallback if GPIO 2 misbehaves on this board.

### Status LED

5 mm red, board-mounted, GPIO 23 through 330 Ω to the anode, cathode to ground. ~4 mA at 3.3 V — clearly visible without being glaring in a dark layout room. Standard `LED_D5.0mm` footprint, 2.54 mm lead spacing, no measurement needed.

Placement: board edge, opposite side from the terminal blocks, clear of the ESP32 body and the LM2596. A shadowed indicator is a useless one. Silkscreen `STATUS` and mark the cathode flat.

Red rather than blue or white deliberately — Vf ≈ 2.0 V leaves adequate headroom across the resistor at 3.3 V, where a 3.0 V Vf would make brightness swing with supply tolerance.

Optional: a two-pole footprint in parallel, left unpopulated, so a fascia indicator can be added later without a board revision. Costs nothing as DNP.

### Mounting

4 × M3, one per corner, inset 5 mm from each edge.

**Unplated.** Plated-and-grounded holes would bond the ground plane to whatever the board is fastened to, and with track power and a booster in the same room that's an invitation to a ground loop. Unplated keeps the electrical system isolated.

~6 mm diameter keep-out clear of copper and components around each hole — screw heads and washers overhang, and the first washer fitted will otherwise short something.

M3 nylon standoffs (on hand). Nylon rather than metal, for the same isolation reason.

### No enclosure

Board mounts open on standoffs. Two consequences:

- Label the 12 V block clearly on silkscreen (`12V +` / `−`). It's exposed, under the layout, near a track bus that may carry more current.
- Orient the board so terminal screws face outward — terminations will be made as much by feel as by sight.

---

## 6. Open item 3a — footprint measurement checklist

**Not yet done. Prerequisite for layout.**

Digital calipers. Measure across N pitches and divide by N; a single 2.54 mm gap measured alone gives useless precision.

| Part | Measure | Why it matters |
|---|---|---|
| ESP32 dev board | Row centre-to-centre; pitch across all 14 gaps; body L×W; pin length below board | Row spacing is the one that bites — this board already didn't fit a breadboard cleanly |
| ESP32 USB connector | Type (micro-B on the ELEGOO), overhang past board edge, plug clearance | May need an edge cutout |
| TB6612 × 4 | Row spacing, pins per row, **pin order against silkscreen — all four modules** | Generic modules vary between batches. This is the one that produces a board where nothing works and everything measures correct. |
| Screw terminals | Pitch verified across 8+ poles; body depth and height; pin diameter; wire entry direction; whether they interlock, and the pitch across an interlock joint | Pin diameter sets drill size; depth eats board area behind the edge |
| DIP-14 sockets | Row spacing, lead thickness | Nominal 0.3", verify |
| 470 µF cap | Body diameter, lead spacing, height | Lead spacing varies 3.5–7.5 mm at this size |
| LM2596 module | Outline, mounting hole spacing, **pot accessibility when seated** | Needs re-trimming in place |

Record results here as a table when measured.

---

## 7. Fab (open item 3b)

Nothing on this board is fine-pitch and the 12 V rail carries ~400 mA. Default 6/6 mil rules at any house are far more than adequate. **Choose on cost and lead time, not capability.**

Target outline: under 100 × 100 mm, where the low-cost price breaks sit.

---

## 8. Build order

Test-first in spirit — catch errors at instance one, not eight.

1. Complete 3a measurements; record above.
2. Build or verify footprints against measured parts.
3. Draw the channel sheet. ERC clean with a **single** instance.
4. Instantiate the remaining seven.
5. Draw the root sheet. Full ERC.
6. Layout: mounting holes and terminal edges placed first, then modules, then routing.
7. **Print the layout 1:1 and place real parts on the paper.** Catches footprint errors for the cost of a sheet of paper.
8. Gerber review, then order.

---

## 9. ERC notes

- `PWR_FLAG` on `+12V`, `+5V` and `+3V3`. None of them originates from a symbol KiCad recognises as a source — the LM2596 and the ESP32 are both modules.
- Add the four mounting holes to the schematic now. Retrofitting them after layout means moving traces.

---

## 10. Deferred to Rev B

| Item | Why not now |
|---|---|
| Bare SSOP-24 TB6612FNG instead of modules | Cheaper and more compact, but 0.65 mm pitch soldering and support passives |
| Two-sensor feedback | Second GPIO and terminal pole per channel |
| Fascia indicator terminal | DNP footprint reserved on Rev A |
| Frog polarity routing | Deliberate — keeps track current off the board |
