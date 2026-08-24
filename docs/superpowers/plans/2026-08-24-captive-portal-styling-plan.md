# Captive Portal Styling + Pre-fill Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the captive portal's bare, static, never-pre-filled setup form with a styled page that reflects previously-saved config, per `docs/superpowers/specs/2026-08-24-captive-portal-styling-design.md`.

**Architecture:** Split HTML rendering out of the Arduino-only `CaptivePortalServer` into a new pure, host-testable domain class `SetupFormRenderer`, fed by a new `WebFormCommissioningAdapter::currentValues()` method that maps the saved `NodeConfig` into the existing `WebFormSubmission` struct. `CaptivePortalServer` shrinks to thin glue: call `currentValues()`, call `render()`, send the result; and join 4 new broker-host-octet form fields into the one `brokerHost` string on submit.

**Tech Stack:** C++17, PlatformIO (`native` env for host tests via Catch2, `esp32dev` env for the real build), Arduino ESP32 core (`WebServer`, `DNSServer`).

## Global Constraints

- Domain code (anything under `lib/McsCore/src/domain/`) must compile and run under the `native` PlatformIO environment without `Arduino.h` — `SetupFormRenderer` must have **no** `#ifdef ARDUINO` guard and **no** Arduino includes.
- No blocking calls (`delay()`) — not applicable to this plan (no new timing logic).
- Native tests: run via `pio test -e native -f test_<name>` from the repo root.
- ESP32 build check: `pio run -e esp32dev` from the repo root.
- Every commit must use Arlo's Commit Notation (ACN): `<risk symbol> <intention letter> <short description>`, via this repo's `/arlo-commits` skill — do not hand-write commit messages.
- New/modified files use the project's existing style: 4-space indent, braces on their own line, header-only classes (methods defined inline in the class body) for small domain/adapter classes — matches `CommandLineParser`, `WebFormCommissioningAdapter`.

---

### Task 1: `WebFormCommissioningAdapter::currentValues()`

**Files:**
- Modify: `lib/McsCore/src/adapters/WebFormCommissioningAdapter.h`
- Test: `test/test_web_form_commissioning_adapter/test_main.cpp` (existing file — add new `TEST_CASE`s)

**Interfaces:**
- Consumes: `CommissioningSession::draft() const` → `const NodeConfig&` (existing). `NodeConfig::id()` → `NodeId`, `NodeId::value()` → `int`. `NodeConfig::wifi()` → `const WifiCredentials&` (`.ssid()`, `.password()` → `const std::string&`). `NodeConfig::broker()` → `const BrokerAddress&` (`.host()` → `const std::string&`, `.port()` → `int`).
- Produces: `WebFormSubmission WebFormCommissioningAdapter::currentValues() const` — used by Task 5's `CaptivePortalServer::handleRoot()`.

- [ ] **Step 1: Write the failing tests**

Append to `test/test_web_form_commissioning_adapter/test_main.cpp` (before the final closing of the file, after the existing `TEST_CASE`s):

```cpp
TEST_CASE("WebFormCommissioningAdapter currentValues reflects an empty factory-default config")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.nodeId == "");
    REQUIRE(values.wifiSsid == "");
    REQUIRE(values.wifiPassword == "");
    REQUIRE(values.brokerHost == "");
    REQUIRE(values.brokerPort == "1883");
}

TEST_CASE("WebFormCommissioningAdapter currentValues reflects a previously saved config")
{
    FakeConfigStore store;
    Orientation orientation = Orientation::normal();
    Duration settle(50);
    Duration timeout(200);
    std::array<TurnoutConfig, 8> seedTurnouts{
        TurnoutConfig(TurnoutId(1), 21, 31, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(2), 22, 32, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(3), 23, 33, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(4), 24, 34, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(5), 25, 35, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(6), 26, 36, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(7), 27, 37, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(8), 28, 38, orientation, settle, timeout)};
    NodeConfig seed(NodeId(5), WifiCredentials("Layout Room", "blue caboose 42"), BrokerAddress("192.168.1.50", 1883), seedTurnouts);
    store.save(seed);

    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.nodeId == "5");
    REQUIRE(values.wifiSsid == "Layout Room");
    REQUIRE(values.wifiPassword == "blue caboose 42");
    REQUIRE(values.brokerHost == "192.168.1.50");
    REQUIRE(values.brokerPort == "1883");
}
```

No new `#include`s are needed in the test file — `NodeConfig`, `NodeId`, `WifiCredentials`, `BrokerAddress`, `TurnoutConfig`, `TurnoutId`, `Orientation`, `Duration` are already included.

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: FAIL to compile — `'class WebFormCommissioningAdapter' has no member named 'currentValues'`

- [ ] **Step 3: Implement `currentValues()`**

In `lib/McsCore/src/adapters/WebFormCommissioningAdapter.h`, add the include and the method. The include list currently reads:

```cpp
#include "domain/WebFormSubmission.h"
#include "domain/CommissioningSession.h"
#include "domain/CommandLineParser.h"
#include "domain/ParsedCommand.h"
```

Add `#include "domain/NodeConfig.h"` to that list (don't rely on the transitive include via `CommissioningSession.h`).

Add this public method to `class WebFormCommissioningAdapter`, alongside `submit()` and `rebootRequested()`:

```cpp
    WebFormSubmission currentValues() const
    {
        const NodeConfig& config = session_.draft();

        WebFormSubmission form;
        form.nodeId = config.id().value() == 0 ? "" : std::to_string(config.id().value());
        form.wifiSsid = config.wifi().ssid();
        form.wifiPassword = config.wifi().password();
        form.brokerHost = config.broker().host();
        form.brokerPort = std::to_string(config.broker().port());
        return form;
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: PASS — all `TEST_CASE`s in this file, including the 2 new ones.

- [ ] **Step 5: Commit**

Use the `/arlo-commits` skill. This is `^ F` (new capability, matching tests added, well under 8 lines of production code) — `Add WebFormCommissioningAdapter::currentValues()`.

---

### Task 2: `SetupFormRenderer` — IPv4 octet splitting

**Files:**
- Create: `lib/McsCore/src/domain/SetupFormRenderer.h`
- Test: `test/test_setup_form_renderer/test_main.cpp` (new file)

**Interfaces:**
- Consumes: nothing from other tasks yet.
- Produces: `static std::array<std::string, 4> SetupFormRenderer::splitIpv4Octets(const std::string& host)` — used internally by `render()` in Task 4, and directly by `CaptivePortalServer`'s manual verification in Task 6.

- [ ] **Step 1: Write the failing tests**

Create `test/test_setup_form_renderer/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

#include "domain/SetupFormRenderer.h"

TEST_CASE("splitIpv4Octets splits a valid dotted quad")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("192.168.1.13");

    REQUIRE(octets[0] == "192");
    REQUIRE(octets[1] == "168");
    REQUIRE(octets[2] == "1");
    REQUIRE(octets[3] == "13");
}

TEST_CASE("splitIpv4Octets returns all-blank for an empty host")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}

TEST_CASE("splitIpv4Octets returns all-blank for a hostname")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("mqtt.local");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}

TEST_CASE("splitIpv4Octets returns all-blank when an octet is out of range")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("192.168.1.999");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}

TEST_CASE("splitIpv4Octets returns all-blank for the wrong number of parts")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("192.168.1");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: FAIL to compile — `domain/SetupFormRenderer.h` doesn't exist yet.

- [ ] **Step 3: Implement `SetupFormRenderer` with `splitIpv4Octets`**

Create `lib/McsCore/src/domain/SetupFormRenderer.h`:

```cpp
#pragma once

#include <array>
#include <cctype>
#include <string>

#include "domain/WebFormSubmission.h"

class SetupFormRenderer
{
public:
    // Splits a dotted-quad IPv4 string into its 4 octets. Returns
    // {"", "", "", ""} unless the string is exactly 4 dot-separated numeric
    // parts, each in 0-255 - used to pre-fill the setup form's broker-host
    // boxes without ever showing a misleading default for an unset or
    // non-IPv4 (e.g. hostname) saved value.
    static std::array<std::string, 4> splitIpv4Octets(const std::string& host)
    {
        std::array<std::string, 4> empty{"", "", "", ""};
        std::array<std::string, 4> parts;
        size_t partIndex = 0;
        std::string current;

        for (char c : host)
        {
            if (c == '.')
            {
                if (partIndex >= 4)
                {
                    return empty;
                }
                parts[partIndex++] = current;
                current.clear();
            }
            else
            {
                current += c;
            }
        }

        if (partIndex != 3)
        {
            return empty;
        }
        parts[partIndex++] = current;

        for (const std::string& part : parts)
        {
            if (part.empty() || part.size() > 3)
            {
                return empty;
            }
            for (char c : part)
            {
                if (!std::isdigit(static_cast<unsigned char>(c)))
                {
                    return empty;
                }
            }
            if (std::stoi(part) > 255)
            {
                return empty;
            }
        }

        return parts;
    }
};
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: PASS — all 5 `TEST_CASE`s.

- [ ] **Step 5: Commit**

Use the `/arlo-commits` skill. This is `^ F` (new capability, matching tests, pure logic) — `Add SetupFormRenderer::splitIpv4Octets`.

---

### Task 3: `SetupFormRenderer` — HTML escaping

**Files:**
- Modify: `lib/McsCore/src/domain/SetupFormRenderer.h`
- Test: `test/test_setup_form_renderer/test_main.cpp`

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces: `static std::string SetupFormRenderer::escapeHtml(const std::string& text)` — used internally by `render()` in Task 4.

- [ ] **Step 1: Write the failing tests**

Append to `test/test_setup_form_renderer/test_main.cpp`:

```cpp
TEST_CASE("escapeHtml escapes ampersand, angle brackets, and quotes")
{
    std::string escaped = SetupFormRenderer::escapeHtml("Tom & Jerry's \"Layout\" <shed>");

    REQUIRE(escaped == "Tom &amp; Jerry&#39;s &quot;Layout&quot; &lt;shed&gt;");
}

TEST_CASE("escapeHtml leaves ordinary text unchanged")
{
    REQUIRE(SetupFormRenderer::escapeHtml("Layout Room") == "Layout Room");
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: FAIL to compile — `'class SetupFormRenderer' has no member named 'escapeHtml'`

- [ ] **Step 3: Implement `escapeHtml`**

Add this public method to `class SetupFormRenderer` in `lib/McsCore/src/domain/SetupFormRenderer.h`, alongside `splitIpv4Octets`:

```cpp
    // A saved SSID/password/broker value could contain HTML-significant
    // characters - escape before embedding in an attribute value so it
    // can't break the page or inject markup back into the same form.
    static std::string escapeHtml(const std::string& text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (char c : text)
        {
            switch (c)
            {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&#39;";
                break;
            default:
                escaped += c;
            }
        }
        return escaped;
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: PASS — all 7 `TEST_CASE`s (5 from Task 2 + 2 new).

- [ ] **Step 5: Commit**

Use the `/arlo-commits` skill. This is `^ F` — `Add SetupFormRenderer::escapeHtml`.

---

### Task 4: `SetupFormRenderer` — full page rendering

**Files:**
- Modify: `lib/McsCore/src/domain/SetupFormRenderer.h`
- Test: `test/test_setup_form_renderer/test_main.cpp`

**Interfaces:**
- Consumes: `WebFormSubmission` fields (`nodeId`, `wifiSsid`, `wifiPassword`, `brokerHost`, `brokerPort` — all `std::string`, from `domain/WebFormSubmission.h`, already included). `splitIpv4Octets` and `escapeHtml` from Tasks 2 & 3 (same class).
- Produces: `static std::string SetupFormRenderer::render(const WebFormSubmission& values)` — used by `CaptivePortalServer::handleRoot()` in Task 5.

- [ ] **Step 1: Write the failing tests**

Append to `test/test_setup_form_renderer/test_main.cpp`:

```cpp
TEST_CASE("render selects the placeholder when node id is empty")
{
    WebFormSubmission values;
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("<option value='' disabled selected hidden>-- select --</option>") != std::string::npos);
    REQUIRE(html.find("<option value='5' selected>5</option>") == std::string::npos);
}

TEST_CASE("render marks the saved node id as selected")
{
    WebFormSubmission values;
    values.nodeId = "5";
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("<option value='5' selected>5</option>") != std::string::npos);
    REQUIRE(html.find("disabled selected hidden") == std::string::npos);
}

TEST_CASE("render HTML-escapes the WiFi SSID and password")
{
    WebFormSubmission values;
    values.wifiSsid = "Tom & Jerry's";
    values.wifiPassword = "pa\"ss";
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("name='wifi_ssid' value='Tom &amp; Jerry&#39;s'") != std::string::npos);
    REQUIRE(html.find("name='wifi_password' type='password' value='pa&quot;ss'") != std::string::npos);
}

TEST_CASE("render pre-fills the broker host octet boxes from a saved IP")
{
    WebFormSubmission values;
    values.brokerHost = "192.168.1.13";
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("name='broker_host_1' type='number' min='0' max='255' value='192'") != std::string::npos);
    REQUIRE(html.find("name='broker_host_4' type='number' min='0' max='255' value='13'") != std::string::npos);
}

TEST_CASE("render leaves broker host octet boxes blank for an invalid saved host")
{
    WebFormSubmission values;
    values.brokerHost = "not-an-ip";
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("name='broker_host_1' type='number' min='0' max='255' value=''") != std::string::npos);
}

TEST_CASE("render pre-fills the broker port")
{
    WebFormSubmission values;
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("name='broker_port' type='number' value='1883'") != std::string::npos);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: FAIL to compile — `'class SetupFormRenderer' has no member named 'render'`

- [ ] **Step 3: Implement `render()`**

Add these as public/private members of `class SetupFormRenderer` in `lib/McsCore/src/domain/SetupFormRenderer.h`. `render()` and `renderIdOptions()` are new; `escapeHtml`/`splitIpv4Octets` already exist from Tasks 2-3.

```cpp
public:
    static std::string render(const WebFormSubmission& values)
    {
        std::array<std::string, 4> octets = splitIpv4Octets(values.brokerHost);

        std::string html;
        html += "<!DOCTYPE html><html><head>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>" + kStyle + "</style></head><body>";
        html += "<div class='card'><h1>Tortoise Setup</h1>";
        html += "<p class='subtitle'>Configure this board's network settings</p>";
        html += "<form method='POST' action='/submit'>";
        html += "<label>Node ID</label><select name='id'>" + renderIdOptions(values.nodeId) + "</select>";
        html += "<label>WiFi SSID</label><input name='wifi_ssid' value='" + escapeHtml(values.wifiSsid) + "'>";
        html += "<label>WiFi Password</label><input name='wifi_password' type='password' value='"
            + escapeHtml(values.wifiPassword) + "'>";
        html += "<label>Broker Host</label><div class='octets'>";
        for (size_t i = 0; i < octets.size(); ++i)
        {
            if (i > 0)
            {
                html += "<span>.</span>";
            }
            html += "<input name='broker_host_" + std::to_string(i + 1)
                + "' type='number' min='0' max='255' value='" + escapeHtml(octets[i]) + "'>";
        }
        html += "</div>";
        html += "<label>Broker Port</label><input name='broker_port' type='number' value='"
            + escapeHtml(values.brokerPort) + "'>";
        html += "<button type='submit'>Save</button>";
        html += "</form></div></body></html>";
        return html;
    }

private:
    static std::string renderIdOptions(const std::string& selectedId)
    {
        std::string options;
        if (selectedId.empty())
        {
            options += "<option value='' disabled selected hidden>-- select --</option>";
        }
        for (int i = 1; i <= 16; ++i)
        {
            std::string value = std::to_string(i);
            options += "<option value='" + value + "'" + (value == selectedId ? " selected" : "") + ">" + value + "</option>";
        }
        return options;
    }

    static inline const std::string kStyle =
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
        "background:#f2f4f7;margin:0;padding:24px 16px;color:#1f2933;}"
        ".card{max-width:420px;margin:0 auto;background:#fff;border-radius:12px;"
        "box-shadow:0 1px 3px rgba(0,0,0,0.1);padding:24px;}"
        "h1{font-size:1.25rem;margin:0 0 4px;}"
        ".subtitle{color:#6b7280;font-size:0.875rem;margin:0 0 20px;}"
        "label{display:block;font-size:0.8rem;font-weight:600;color:#374151;margin:16px 0 4px;}"
        "input,select{width:100%;box-sizing:border-box;padding:8px 10px;border:1px solid #d1d5db;"
        "border-radius:6px;font-size:0.95rem;}"
        ".octets{display:flex;align-items:center;gap:6px;}"
        ".octets input{width:4rem;text-align:center;flex:none;}"
        ".octets span{color:#9ca3af;}"
        "button{margin-top:24px;width:100%;padding:10px;background:#2563eb;color:#fff;"
        "border:none;border-radius:6px;font-size:1rem;font-weight:600;cursor:pointer;}"
        "button:hover{background:#1d4ed8;}";
```

Note: `renderIdOptions` and `kStyle` must be placed in a `private:` section that comes *after* the `public:` section containing `render`, `splitIpv4Octets`, and `escapeHtml` — matching the existing single-`public:`-block-then-`private:`-block style used elsewhere in this codebase (e.g. `WiFiLink`). Since `splitIpv4Octets` and `escapeHtml` were added as `public:` members in Tasks 2-3, keep them there; only `renderIdOptions` and `kStyle` are genuinely private (never called from outside the class or tests).

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: PASS — all 13 `TEST_CASE`s (7 from Tasks 2-3 + 6 new).

- [ ] **Step 5: Commit**

Use the `/arlo-commits` skill. This is `^ F` (matching tests, but note the production diff for this step alone is well over 8 lines — for a pure-logic `F` with full test coverage and a passing native suite, `^` still applies since the 8-line cap is specific to hand-verified F/B changes; this one is fully exercised by 13 passing `TEST_CASE`s). If judged more conservatively, `!` is also defensible — let the `/arlo-commits` skill make the final call. — `Add SetupFormRenderer::render`.

---

### Task 5: Wire `SetupFormRenderer` into `CaptivePortalServer`

**Files:**
- Modify: `lib/McsCore/src/adapters/CaptivePortalServer.h`

**Interfaces:**
- Consumes: `WebFormCommissioningAdapter::currentValues() const` → `WebFormSubmission` (Task 1). `SetupFormRenderer::render(const WebFormSubmission&)` → `std::string` (Task 4).
- Produces: nothing new for later tasks — this is the final wiring point.

This file is `#ifdef ARDUINO`-gated (real ESP32 `WebServer`/`DNSServer` hardware glue) and has no native test coverage today, matching the rest of this file's existing untested-passthrough style. Verification is the `esp32dev` build.

- [ ] **Step 1: Add the include**

In `lib/McsCore/src/adapters/CaptivePortalServer.h`, add to the include list (after `"adapters/WebFormCommissioningAdapter.h"`):

```cpp
#include "domain/SetupFormRenderer.h"
```

- [ ] **Step 2: Replace `handleRoot()` and remove `kFormHtml`**

Replace:

```cpp
    void handleRoot()
    {
        webServer_.send(200, "text/html", kFormHtml);
    }
```

with:

```cpp
    void handleRoot()
    {
        webServer_.send(200, "text/html", SetupFormRenderer::render(adapter_.currentValues()).c_str());
    }
```

Delete the entire `kFormHtml` constant (the `static constexpr const char* kFormHtml = ...;` block, including its closing `;`).

- [ ] **Step 3: Read the 4 broker-host octet fields in `readForm()`**

Replace this line in `readForm()`:

```cpp
        form.brokerHost = webServer_.arg("broker_host").c_str();
```

with:

```cpp
        std::string brokerHostOctet1 = webServer_.arg("broker_host_1").c_str();
        std::string brokerHostOctet2 = webServer_.arg("broker_host_2").c_str();
        std::string brokerHostOctet3 = webServer_.arg("broker_host_3").c_str();
        std::string brokerHostOctet4 = webServer_.arg("broker_host_4").c_str();
        form.brokerHost = brokerHostOctet1 + "." + brokerHostOctet2 + "." + brokerHostOctet3 + "." + brokerHostOctet4;
```

- [ ] **Step 4: Build for ESP32 to verify it compiles**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`

- [ ] **Step 5: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: all test cases pass (this task touches no native-tested code, so this should be unaffected, but confirms nothing else broke).

- [ ] **Step 6: Commit**

Use the `/arlo-commits` skill. This is `! F` (real behavior change to an Arduino-only file with no native test coverage — matches this repo's existing risk posture for `CaptivePortalServer`) — `Wire SetupFormRenderer into CaptivePortalServer`.

---

### Task 6: Manual verification on hardware (recommended)

**Files:** none — verification only.

This is a UI change with no automated coverage for the actual served page (Task 5 is Arduino-only). Recommended before considering this done, per the design spec.

- [ ] **Step 1: Flash the board in wireless-setup mode**

`pio run -e esp32dev --target upload`, then power on the board while holding BOOT (or trigger the runtime setup-mode gesture if already commissioned — hold BOOT ~3s during normal operation, then release) to enter `BootMode::WirelessSetup`.

- [ ] **Step 2: Connect and open the captive portal**

Connect a phone or laptop to the board's `Tortoise-Setup-XXXX` AP; the captive portal should open automatically (or browse to the AP's IP).

- [ ] **Step 3: Visually confirm styling**

Confirm: centered card layout, readable spacing/typography, the node id field is a dropdown (not a text box), and the broker host field is 4 separate boxes with `.` separators between them.

- [ ] **Step 4: Confirm pre-fill on a previously-commissioned board**

If this board has prior saved config (from earlier testing), confirm the WiFi SSID, WiFi password, broker host octets, and broker port fields all show the previously-saved values, and the node id dropdown has the saved id selected.

- [ ] **Step 5: Submit and confirm it still saves correctly**

Change one field (e.g. broker port), submit, and confirm (via `pio device monitor` after reboot, or `mosquitto_sub` on the presence topic) that the node comes back up with the new value applied and the untouched fields unchanged.

No commit for this task — it's verification only. If any step fails, treat it as a new bug: return to systematic-debugging rather than patching ad hoc.

---

## Self-Review

**Spec coverage:**
- Visual style (clean minimal card, embedded `<style>`, no external assets) → Task 4 (`kStyle`).
- WiFi password pre-filled → Task 1 (`currentValues`) + Task 4 (`render` includes `wifi_password` value).
- Node id as 1-16 dropdown with placeholder-when-unset → Task 4 (`renderIdOptions`).
- Broker host as 4 IPv4 octet boxes, blank-on-invalid → Task 2 (`splitIpv4Octets`) + Task 4 (rendering) + Task 5 (`readForm` join).
- `WebFormCommissioningAdapter::currentValues()` → Task 1.
- `SetupFormRenderer` pure/host-testable, no Arduino dependency → Task 2-4 (verified: no `#ifdef ARDUINO`, no Arduino includes anywhere in the plan's `SetupFormRenderer.h` content).
- `CaptivePortalServer` thin-glue wiring, `kFormHtml` removed → Task 5.
- Manual verification recommendation from the spec → Task 6.
- Turnout fields explicitly out of scope → not present in any task. Confirmed no gaps.

**Placeholder scan:** No "TBD"/"TODO"/"handle appropriately" language; every step has complete, concrete code. Task 4's Step 5 commit-risk note is deliberately a judgment call flagged for the `/arlo-commits` skill, not a placeholder.

**Type consistency:** `WebFormSubmission` field names (`nodeId`, `wifiSsid`, `wifiPassword`, `brokerHost`, `brokerPort`) used identically across Tasks 1, 4, and 5. `SetupFormRenderer::render(const WebFormSubmission&)` signature matches its Task 5 call site. `splitIpv4Octets`/`escapeHtml` signatures match their Task 4 call sites exactly.
