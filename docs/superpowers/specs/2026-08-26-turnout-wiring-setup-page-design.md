# Turnout Wiring Fields on the Setup Page — Design

## Purpose

The wireless captive-portal setup page (`SetupFormRenderer`/`CaptivePortalServer`)
only lets a builder configure network settings (node ID, WiFi, broker) —
turnout channel wiring (pin, feedback pin, orientation, settle, timeout) can
currently only be set over a USB serial session, one `turnout <n> pin <gpio>
fb <gpio> orientation <normal|inverted> settle <ms> timeout <ms>` command at
a time. This is error-prone by hand (a whole debugging session,
2026-08-25/26, traced a board's non-functional turnouts to exactly this:
channels left at the factory-default `pin=-1`, invisible until the
bench-serial `show` command was used) and slower than it needs to be for a
builder standing up multiple channels.

This adds turnout wiring fields to the same wireless setup page, for the
builder/technician specifically — not a change to what an end customer needs
to touch. `docs/first-time-setup.md`'s existing guidance (turnout wiring is
set up ahead of time by the builder, not part of the customer's own setup
flow) stays accurate; this just gives the builder a better tool for that
same step, on the same page, rather than a new customer-facing capability.

## Scope decisions

- **Audience:** builder/technician only. The page stays the same URL/flow a
  customer also uses for network reconfiguration, so the turnout fields are
  tucked behind a collapsed disclosure, not shown open by default.
- **No JavaScript.** Matches the existing captive portal, which is plain
  HTML/CSS today. The collapsed section uses the native HTML5
  `<details>`/`<summary>` element — supported in every mobile browser this
  project targets, zero script.
- **Pin entry is a constrained dropdown, not free text**, for both the
  output pin and the feedback pin, populated from a fixed, documented-safe
  GPIO list (below) — not the full 0–39 range. This closes off the class of
  mistake (wrong/blank pin, strapping-pin conflict) found during the
  2026-08-25/26 debugging session, at the input level rather than relying on
  validation after the fact.
- **Every field defaults to a known-good value** when nothing is saved yet
  for that channel, rather than defaulting to blank: pin/feedback pin
  default to that channel's documented GPIOs (project notes §4), orientation
  defaults to `normal`, settle defaults to `500`, timeout defaults to `4000`.
  All remain fully overridable per channel. This directly targets two
  concrete failures from that same debugging session: an uncommissioned
  channel silently doing nothing (`pin=-1`), and the factory-default
  `timeout=200` faulting every real Tortoise throw before it can physically
  complete.
- **Not changing:** `CaptivePortalServer::readForm()`,
  `WebFormCommissioningAdapter::buildCommandLines()`, `CommandLineParser`,
  `CommissioningSession` — all already correctly handle per-channel
  `pin`/`feedbackPin`/`orientation`/`settleMs`/`timeoutMs` end to end today.
  This design only adds the missing HTML and the missing read-back-for-display
  path.
- **Not changing error handling.** A malformed submission (e.g. a channel
  with a pin selected but feedback left unset) still fails the whole
  submission with a plain-text `ERROR: ...` response, same as an invalid
  broker port does today. Defaulting pin and feedback together makes this
  edge case rarer, but a friendlier error UI is out of scope here.

## Pin lists

Derived from `internal_documents/esp32-turnout-controller-project-notes.md
§4` and confirmed against the actual board/wiring during design (2026-08-26).
Excluded throughout: GPIO 0 (BOOT button), 1/3 (USB serial), 2 (status LED),
5/12/15 (strapping pins), 21/22 (reserved for future I²C), 23 (reserved
status LED pin).

**Output pin** (12 options): `4, 13, 14, 16, 17, 18, 19, 25, 26, 27, 32, 33`

**Feedback pin** (16 options): output list plus the 4 input-only pins —
`4, 13, 14, 16, 17, 18, 19, 25, 26, 27, 32, 33, 34, 35, 36, 39`

**Per-channel documented defaults** (project notes §4 channel table):

| Channel | Default pin | Default feedback pin |
|---|---|---|
| 1 | 13 | 36 |
| 2 | 14 | 39 |
| 3 | 27 | 34 |
| 4 | 26 | 35 |
| 5 | 25 | 16 |
| 6 | 33 | 17 |
| 7 | 32 | 18 |
| 8 | 4 | 19 |

## Components

### `SetupFormRenderer` (modified)

`lib/McsCore/src/domain/SetupFormRenderer.h` gains:

- A private static table of the 8 per-channel `(defaultPin, defaultFeedbackPin)`
  pairs above.
- A private helper to render one channel's field group: two `<select>`
  dropdowns (pin, feedback pin) built from the fixed pin lists above, an
  `orientation` `<select>` (`normal`/`inverted`), and two `<input
  type='number'>` fields (settle, timeout).
- Selection/pre-fill logic per field, mirroring the existing `nodeId`
  placeholder pattern: if `WebFormSubmission`'s value for this channel/field
  is empty, the field shows its default (per-channel GPIO default for
  pin/feedback, `normal` for orientation, `500`/`4000` for settle/timeout);
  otherwise it shows the saved value, selected/pre-filled.
- `render()` adds one `<details><summary>Advanced: Turnout
  Wiring</summary>...</details>` block, placed after the existing broker
  fields, containing all 8 channel groups in order.
- All rendered values continue to go through the existing `escapeHtml`
  helper before being embedded, even though pin/feedback/orientation now
  come from a fixed server-side option list rather than arbitrary user text —
  keeps one consistent escaping discipline across the whole file rather than
  special-casing "safe" fields.

### `WebFormCommissioningAdapter` (modified)

`lib/McsCore/src/adapters/WebFormCommissioningAdapter.h`'s `currentValues()`
gains a loop populating `form.turnouts[i]` from `config.turnouts()[i]` for
all 8 channels (`pin`, `feedbackPin`, `orientation`, `settleMs`, `timeoutMs`
— each converted to its string form, empty for the sentinel `pin == -1`
case, matching how `nodeId` already returns `""` for the sentinel `id == 0`).
`SetupFormRenderer` is the only place that knows about the per-channel
defaults; this method only ever returns what's actually saved (or empty).

No other file changes — `CaptivePortalServer::readForm()`,
`WebFormCommissioningAdapter::submit()`/`buildCommandLines()`, and the
`CommandLineParser`/`CommissioningSession` validation path already handle
the `t{N}_pin` etc. field names and the resulting `turnout N pin ... fb ...`
command lines correctly.

## Data Flow

```
GET / (or after a previous submit)
  → CaptivePortalServer::handleRoot
  → WebFormCommissioningAdapter::currentValues()
      (reads saved NodeConfig, including all 8 turnouts; empty per-channel
      fields where a channel was never commissioned)
  → SetupFormRenderer::render(values)
      (fills in each channel's dropdowns/inputs: saved value if present,
      otherwise the channel's documented default)
  → HTML response, <details> collapsed by default

POST /submit
  → CaptivePortalServer::readForm() (unchanged - already reads t{N}_* fields)
  → WebFormCommissioningAdapter::submit() (unchanged - already builds and
      applies "turnout N pin ... fb ... orientation ... settle ... timeout ..."
      command lines via CommandLineParser/CommissioningSession, skipping any
      channel whose pin is empty)
```

## Testing

- **`test_setup_form_renderer`** (extend existing file): rendered output
  contains a `<details>` block with 8 channel field groups; each channel's
  pin/feedback `<select>` has the correct option list and defaults to that
  channel's documented GPIOs when the submission value is empty, or shows
  the saved value selected when present; orientation defaults to `normal`
  and reflects a saved `inverted` correctly; settle/timeout default to
  `500`/`4000` when empty and reflect saved values otherwise.
- **`test_web_form_commissioning_adapter`** (extend existing file):
  `currentValues()` returns all 5 fields per channel correctly populated
  from a `NodeConfig` with real turnout data, and empty for a channel still
  at the factory-default sentinel.

Both are pure/native-testable — no `#ifdef ARDUINO` involved in either
changed method.

## Docs

No change needed to `docs/first-time-setup.md` — its existing guidance
("turnout wiring... is not configured through this wireless form... set up
ahead of time by the builder") remains accurate; this feature changes *how*
the builder does that setup, not who does it or when.
