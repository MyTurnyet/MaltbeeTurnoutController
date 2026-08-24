# Captive Portal Setup Page: Styling + Pre-fill

## Problem

`CaptivePortalServer` serves a static, unstyled HTML form (`kFormHtml`) for
wireless setup. It has two problems:

1. It looks bare — plain unstyled inputs, no visual hierarchy.
2. It never reflects previously-saved config. Every field starts empty, even
   on a board that's already been commissioned and is just being
   re-configured (e.g. moving to a new WiFi network).

## Scope

This pass covers the fields the form already has: node id, WiFi SSID/
password, broker host/port. The form currently has **no turnout
configuration fields at all** (the backend already accepts them per-turnout
and silently skips absent ones, but the HTML never renders them) — that gap
is out of scope here and stays a bench-serial-only task for now.

## Design decisions

- **Visual style:** clean minimal centered card, neutral gray/blue palette,
  system sans-serif font, mobile-friendly (this page is opened from a
  phone). No external assets/CDN/JS framework — single self-contained HTML
  response with an embedded `<style>` block, matching how the page is
  served today (one `WebServer` route, no static asset serving).
- **WiFi password pre-fill:** pre-filled with the saved value. This is a
  local-only flow (the user is physically connected to the device's own AP
  to reach this page at all), so convenience wins over the usual
  don't-echo-secrets caution.
- **Node id field:** becomes a `<select>` with options 1–16, matching the
  documented design (`docs/software-class-list.md`'s "Entering Setup Mode"
  section already describes this as a dropdown; the current text box was a
  gap). The saved id is marked `selected`; if unset (`NodeId(0)`, the
  factory default), a disabled placeholder option is selected instead.
- **Broker host field:** becomes 4 separate numeric octet boxes
  (`[192] . [168] . [1] . [13]`) instead of one free-text field. This makes
  the broker host **IPv4-only going forward** — hostname entry (e.g.
  `mqtt.local`) is no longer supported through this form. `BrokerAddress`
  itself is untouched (still a plain `host` string); only the form's
  presentation and read-back are affected. If the currently-saved host
  isn't a valid 4-part 0–255 dotted-quad (blank on first boot, or a
  previously-saved hostname from before this change), all 4 boxes render
  blank rather than showing a misleading `0.0.0.0`.

## Architecture

Split the current single Arduino-only `handleRoot()` into a pure,
host-testable rendering piece and a thin hardware-glue piece — the same
separation this codebase already uses for `CommandLineParser` (pure parsing)
vs. `SerialCommissioningAdapter` (hardware glue).

```
NodeConfig (domain)
   |  WebFormCommissioningAdapter::currentValues()
   v
WebFormSubmission (existing struct, already used the other direction
                    by readForm())
   |  SetupFormRenderer::render()   <- new pure domain class
   v
HTML string
   |  CaptivePortalServer::handleRoot()  <- thin Arduino glue, unchanged shape
   v
webServer_.send(200, "text/html", ...)
```

### `WebFormCommissioningAdapter::currentValues() const` (new method)

Maps the session's current draft `NodeConfig` into a `WebFormSubmission` —
the mirror image of what `readForm()`/`submit()` already do in the other
direction. No I/O, pure data mapping:

- `nodeId` — `id().value()` as a string, or `""` if `0` (unset/factory
  default).
- `wifiSsid` / `wifiPassword` — straight from `WifiCredentials`.
- `brokerHost` — straight from `BrokerAddress::host()` (still one dotted
  string; splitting into octets is a rendering concern, not this method's).
- `brokerPort` — `port()` as a string.
- `turnouts` — left default/empty; out of scope (see above).

This class already has no `#ifdef ARDUINO` guard, so this is natively
testable like the rest of it.

### `SetupFormRenderer` (new pure domain class, header-only)

`lib/McsCore/src/domain/SetupFormRenderer.h`, no Arduino dependency, styled
after `CommandLineParser`'s header-only pure-function shape.

`static std::string render(const WebFormSubmission& values)` — builds the
complete HTML page:

- Embedded `<style>` block (clean minimal card, described above).
- Node id `<select>` with 16 options; matches `values.nodeId` against
  `1`..`16` to mark the selected option, or renders the disabled
  placeholder selected if `values.nodeId` is empty.
- WiFi SSID/password `<input>` values, HTML-escaped (a saved SSID/password
  containing `&`, `<`, `>`, or `"` must not break the page or inject
  markup back into a field the same user is about to re-submit).
- Broker host: a private helper splits `values.brokerHost` into 4 octet
  strings — returns `{"", "", "", ""}` unless the string is exactly 4
  dot-separated numeric parts each in 0–255. Rendered as 4
  `<input type="number" min="0" max="255">` boxes named `broker_host_1`
  through `broker_host_4`, with literal `.` characters between them.
- Broker port: single pre-filled `<input type="number">`, `values.brokerPort`
  HTML-escaped like the other free-text fields.

### `CaptivePortalServer` changes

- `handleRoot()`: replaces the static `kFormHtml` send with
  `SetupFormRenderer::render(adapter_.currentValues())`.
- `readForm()`: reads `broker_host_1`..`broker_host_4` instead of a single
  `broker_host` arg, joins them with `.` into `form.brokerHost` — one extra
  line in an already-untested, trivial passthrough method. No change to
  `WebFormSubmission`'s shape or to anything downstream of it
  (`WebFormCommissioningAdapter::submit()`, `CommandLineParser`,
  `BrokerAddress` are all untouched).
- `kFormHtml` constant is removed (superseded by `SetupFormRenderer`).

## Testing

Native TDD coverage, following this project's existing pattern for
`CommandLineParser`:

- `WebFormCommissioningAdapter::currentValues()` — id-zero-means-empty case,
  round-trip of a fully-populated `NodeConfig`.
- `SetupFormRenderer` — id-select rendering (selected option, placeholder-
  selected when empty), HTML-escaping of SSID/password/broker host
  containing special characters, octet-splitting edge cases (valid quad,
  empty host, hostname/garbage string, out-of-range numbers like `"999"`,
  wrong part count).

`CaptivePortalServer`'s one-line join stays untested like the rest of that
file (Arduino-only, verified by the `esp32dev` build only, same posture as
today). No hardware behavior changes here (WiFi/MQTT connection logic is
untouched), so no mandatory flash-and-verify step, but a manual check of the
rendered page is recommended before calling this done since it's a UI
change.
