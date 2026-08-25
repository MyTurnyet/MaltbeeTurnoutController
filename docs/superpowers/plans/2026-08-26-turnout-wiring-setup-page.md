# Turnout Wiring Fields on the Setup Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add per-channel turnout wiring fields (pin, feedback pin, orientation, settle, timeout) to the wireless captive-portal setup page, so a builder can commission turnout wiring there instead of only via bench-serial commands.

**Architecture:** The backend (`WebFormSubmission`, `CaptivePortalServer::readForm()`, `WebFormCommissioningAdapter::buildCommandLines()`, `CommandLineParser`/`CommissioningSession`) already handles per-channel `t{N}_pin`/`t{N}_fb`/`t{N}_orientation`/`t{N}_settle`/`t{N}_timeout` fields end to end. This plan adds the two missing pieces: `SetupFormRenderer::render()` emitting the actual HTML inputs, and `WebFormCommissioningAdapter::currentValues()` reading saved turnout config back out for pre-fill. A small `Orientation::inverted()` accessor is added first since `currentValues()` needs it.

**Tech Stack:** C++17, PlatformIO `native` test environment (Catch2), no JavaScript (plain HTML/CSS, matching the existing captive portal).

## Global Constraints

- No JavaScript anywhere in the rendered page — use HTML5 `<details>`/`<summary>` for the collapsible section (spec, "No JavaScript").
- Output-pin dropdown options (exactly, in this order): `4, 13, 14, 16, 17, 18, 19, 25, 26, 27, 32, 33` (spec, "Pin lists").
- Feedback-pin dropdown options (exactly, in this order): `4, 13, 14, 16, 17, 18, 19, 25, 26, 27, 32, 33, 34, 35, 36, 39` (spec, "Pin lists").
- Per-channel default pins (channel → pin/feedback pin): 1→13/36, 2→14/39, 3→27/34, 4→26/35, 5→25/16, 6→33/17, 7→32/18, 8→4/19 (spec, "Pin lists").
- Orientation default: `normal`. Settle default: `500`. Timeout default: `4000` (spec, "Scope decisions").
- A channel whose pin is unset stays unconfigured (skip-if-empty), unchanged from today (spec, "Not changing").
- All rendered dynamic values go through the existing `escapeHtml` helper (spec, "Components").

---

### Task 1: `Orientation::inverted()` accessor

**Files:**
- Modify: `lib/McsCore/src/domain/Orientation.h`
- Test: `test/test_orientation/test_main.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `bool Orientation::inverted() const` — later used by Task 2 (`WebFormCommissioningAdapter::currentValues()`).

- [ ] **Step 1: Write the failing test**

Append to `test/test_orientation/test_main.cpp`:

```cpp
TEST_CASE("inverted reports whether the orientation is inverted")
{
    REQUIRE_FALSE(Orientation::normal().inverted());
    REQUIRE(Orientation::inverted().inverted());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_orientation`
Expected: build error, `class Orientation has no member named 'inverted'`.

- [ ] **Step 3: Implement**

In `lib/McsCore/src/domain/Orientation.h`, the current public section is:

```cpp
    TurnoutPosition toPosition(Level level) const
    {
        bool isHigh = (level == Level::High);
        bool isThrown = inverted_ ? !isHigh : isHigh;
        return isThrown ? TurnoutPosition::thrown() : TurnoutPosition::closed();
    }

private:
```

Change it to:

```cpp
    TurnoutPosition toPosition(Level level) const
    {
        bool isHigh = (level == Level::High);
        bool isThrown = inverted_ ? !isHigh : isHigh;
        return isThrown ? TurnoutPosition::thrown() : TurnoutPosition::closed();
    }

    bool inverted() const
    {
        return inverted_;
    }

private:
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_orientation`
Expected: `All tests passed`

- [ ] **Step 5: Commit**

Use the `arlo-commits` skill/methodology (per `CLAUDE.md`) rather than a hand-written message: this is a `^ F` (small, ≤8 LoC, fully covered by a new matching unit test) — e.g. `^ F Add Orientation::inverted accessor`.

```bash
git add lib/McsCore/src/domain/Orientation.h test/test_orientation/test_main.cpp
```

Then commit with the composed ACN message.

---

### Task 2: `WebFormCommissioningAdapter::currentValues()` populates turnout fields

**Files:**
- Modify: `lib/McsCore/src/adapters/WebFormCommissioningAdapter.h`
- Test: `test/test_web_form_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes: `Orientation::inverted() const` (Task 1).
- Produces: `WebFormCommissioningAdapter::currentValues()` now fully populates `form.turnouts[8]` — later consumed by `CaptivePortalServer::handleRoot()` at runtime (already wired, no code change needed there).

**Note:** an uncommissioned channel (`outputPin() == -1`, the factory-default sentinel) must leave **all five** of that channel's fields (`pin`, `feedbackPin`, `orientation`, `settleMs`, `timeoutMs`) as empty strings — not just `pin`/`feedbackPin`. This matters: `TurnoutConfig`'s factory default still carries real `settle=50`/`timeout=200` values alongside the `-1` sentinel pins, and if those leaked through as non-empty, `SetupFormRenderer` (Task 3) would show the old `200`ms timeout footgun instead of its own `500`/`4000` defaults for a never-configured channel. Skipping population entirely for `outputPin() == -1` relies on `WebFormTurnoutField`'s default-constructed `std::string` members already being `""`.

- [ ] **Step 1: Write the failing tests**

In `test/test_web_form_commissioning_adapter/test_main.cpp`, extend the existing `TEST_CASE("WebFormCommissioningAdapter currentValues reflects an empty factory-default config")` (replace its body):

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
    REQUIRE(values.turnouts[0].pin == "");
    REQUIRE(values.turnouts[0].feedbackPin == "");
    REQUIRE(values.turnouts[0].orientation == "");
    REQUIRE(values.turnouts[0].settleMs == "");
    REQUIRE(values.turnouts[0].timeoutMs == "");
    REQUIRE(values.turnouts[7].pin == "");
}
```

And extend the existing `TEST_CASE("WebFormCommissioningAdapter currentValues reflects a previously saved config")` (replace its body, same seed data as today, just more assertions):

```cpp
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
    REQUIRE(values.turnouts[0].pin == "21");
    REQUIRE(values.turnouts[0].feedbackPin == "31");
    REQUIRE(values.turnouts[0].orientation == "normal");
    REQUIRE(values.turnouts[0].settleMs == "50");
    REQUIRE(values.turnouts[0].timeoutMs == "200");
    REQUIRE(values.turnouts[7].pin == "28");
    REQUIRE(values.turnouts[7].feedbackPin == "38");
}
```

Add one new test case for the inverted-orientation path, after the one above:

```cpp
TEST_CASE("WebFormCommissioningAdapter currentValues reflects a saved inverted orientation")
{
    FakeConfigStore store;
    Duration settle(50);
    Duration timeout(200);
    std::array<TurnoutConfig, 8> seedTurnouts{
        TurnoutConfig(TurnoutId(1), 21, 31, Orientation::inverted(), settle, timeout),
        TurnoutConfig(TurnoutId(2), 22, 32, Orientation::normal(), settle, timeout),
        TurnoutConfig(TurnoutId(3), 23, 33, Orientation::normal(), settle, timeout),
        TurnoutConfig(TurnoutId(4), 24, 34, Orientation::normal(), settle, timeout),
        TurnoutConfig(TurnoutId(5), 25, 35, Orientation::normal(), settle, timeout),
        TurnoutConfig(TurnoutId(6), 26, 36, Orientation::normal(), settle, timeout),
        TurnoutConfig(TurnoutId(7), 27, 37, Orientation::normal(), settle, timeout),
        TurnoutConfig(TurnoutId(8), 28, 38, Orientation::normal(), settle, timeout)};
    NodeConfig seed(NodeId(5), WifiCredentials("Layout Room", "blue caboose 42"), BrokerAddress("192.168.1.50", 1883), seedTurnouts);
    store.save(seed);

    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.turnouts[0].orientation == "inverted");
    REQUIRE(values.turnouts[1].orientation == "normal");
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: the two extended tests FAIL (turnout fields all read `""` since `currentValues()` doesn't populate them yet); the new inverted-orientation test also FAILS the same way.

- [ ] **Step 3: Implement**

In `lib/McsCore/src/adapters/WebFormCommissioningAdapter.h`, the current `currentValues()` is:

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

Change it to:

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

        for (size_t i = 0; i < form.turnouts.size(); ++i)
        {
            const TurnoutConfig& turnout = config.turnouts()[i];
            if (turnout.outputPin() == -1)
            {
                continue;
            }

            form.turnouts[i].pin = std::to_string(turnout.outputPin());
            form.turnouts[i].feedbackPin = std::to_string(turnout.feedbackPin());
            form.turnouts[i].orientation = turnout.orientation().inverted() ? "inverted" : "normal";
            form.turnouts[i].settleMs = std::to_string(turnout.settleDuration().milliseconds());
            form.turnouts[i].timeoutMs = std::to_string(turnout.movementTimeout().milliseconds());
        }

        return form;
    }
```

This file already includes `"domain/NodeConfig.h"`; `NodeConfig::turnouts()` returns `const std::array<TurnoutConfig, 8>&`, and `TurnoutConfig` is already visible transitively — no new `#include` needed (confirm by building in Step 4; if the compiler can't find `TurnoutConfig`, add `#include "domain/TurnoutConfig.h"` at the top alongside the existing includes).

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: `All tests passed`

- [ ] **Step 5: Commit**

Use the `arlo-commits` skill/methodology. This is a `! F` (real new behavior, matching tests, but >8 LoC) — e.g. `! F Populate turnout fields in WebFormCommissioningAdapter::currentValues`.

```bash
git add lib/McsCore/src/adapters/WebFormCommissioningAdapter.h test/test_web_form_commissioning_adapter/test_main.cpp
```

Then commit with the composed ACN message.

---

### Task 3: `SetupFormRenderer` renders turnout wiring fields

**Files:**
- Modify: `lib/McsCore/src/domain/SetupFormRenderer.h`
- Test: `test/test_setup_form_renderer/test_main.cpp`

**Interfaces:**
- Consumes: `WebFormSubmission::turnouts` (`std::array<WebFormTurnoutField, 8>`, already exists) — reads whatever `WebFormCommissioningAdapter::currentValues()` (Task 2) or a raw `WebFormSubmission` supplies.
- Produces: HTML `<select name='t{N}_pin'>`, `<select name='t{N}_fb'>`, `<select name='t{N}_orientation'>`, `<input name='t{N}_settle'>`, `<input name='t{N}_timeout'>` for `N` in 1..8 — already consumed today by `CaptivePortalServer::readForm()` (no change needed there, it already reads these exact field names).

This task is independent of Tasks 1–2 (no dependency on `Orientation::inverted()` or `currentValues()` — it only formats whatever strings are already in `WebFormTurnoutField`).

- [ ] **Step 1: Write the failing tests**

Append to `test/test_setup_form_renderer/test_main.cpp`. First add a small scoping helper in an anonymous namespace right after the existing `#include` block (before the first `TEST_CASE`) — needed because `<option value='normal' selected>` etc. text is identical across all 8 channels when unset, so tests must scope to one channel's `<select>...</select>` rather than searching the whole document:

```cpp
namespace
{
std::string extractSelect(const std::string& html, const std::string& selectName)
{
    std::string marker = "name='" + selectName + "'>";
    size_t start = html.find(marker);
    if (start == std::string::npos)
    {
        return "";
    }
    size_t end = html.find("</select>", start);
    return html.substr(start, end - start);
}
}
```

Then append these test cases at the end of the file:

```cpp
TEST_CASE("render includes a collapsed Advanced Turnout Wiring section")
{
    WebFormSubmission values;
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("<details>") != std::string::npos);
    REQUIRE(html.find("<summary>Advanced: Turnout Wiring</summary>") != std::string::npos);
}

TEST_CASE("render defaults each channel's pin dropdown to its documented GPIO when unset")
{
    WebFormSubmission values;
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(extractSelect(html, "t1_pin").find("<option value='13' selected>13</option>") != std::string::npos);
    REQUIRE(extractSelect(html, "t8_pin").find("<option value='4' selected>4</option>") != std::string::npos);
}

TEST_CASE("render defaults each channel's feedback pin dropdown to its documented GPIO when unset")
{
    WebFormSubmission values;
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(extractSelect(html, "t1_fb").find("<option value='36' selected>36</option>") != std::string::npos);
    REQUIRE(extractSelect(html, "t8_fb").find("<option value='19' selected>19</option>") != std::string::npos);
}

TEST_CASE("render selects a saved pin over the documented default")
{
    WebFormSubmission values;
    values.brokerPort = "1883";
    values.turnouts[0].pin = "19";

    std::string html = SetupFormRenderer::render(values);

    std::string pinSelect = extractSelect(html, "t1_pin");
    REQUIRE(pinSelect.find("<option value='19' selected>19</option>") != std::string::npos);
    REQUIRE(pinSelect.find("<option value='13' selected>13</option>") == std::string::npos);
}

TEST_CASE("render defaults orientation to normal when unset and reflects a saved inverted value")
{
    WebFormSubmission values;
    values.brokerPort = "1883";
    values.turnouts[1].orientation = "inverted";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(extractSelect(html, "t1_orientation").find("<option value='normal' selected>normal</option>") != std::string::npos);
    REQUIRE(extractSelect(html, "t2_orientation").find("<option value='inverted' selected>inverted</option>") != std::string::npos);
}

TEST_CASE("render defaults settle and timeout when unset and reflects saved values")
{
    WebFormSubmission values;
    values.brokerPort = "1883";
    values.turnouts[2].settleMs = "750";
    values.turnouts[2].timeoutMs = "6000";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("name='t1_settle' type='number' value='500'") != std::string::npos);
    REQUIRE(html.find("name='t1_timeout' type='number' value='4000'") != std::string::npos);
    REQUIRE(html.find("name='t3_settle' type='number' value='750'") != std::string::npos);
    REQUIRE(html.find("name='t3_timeout' type='number' value='6000'") != std::string::npos);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: all 6 new tests FAIL — none of `t1_pin`/`t1_fb`/`t1_orientation`/`t1_settle`/`t1_timeout`/the `<details>` block exist in the current output.

- [ ] **Step 3: Implement**

In `lib/McsCore/src/domain/SetupFormRenderer.h`:

**3a.** Add `#include <vector>` to the top include block (after `#include <cctype>`):

```cpp
#pragma once

#include <array>
#include <cctype>
#include <string>
#include <vector>

#include "domain/WebFormSubmission.h"
```

**3b.** Insert the turnout wiring section into `render()`, right before the Save button. The current relevant lines are:

```cpp
        html += "<label>Broker Port</label><input name='broker_port' type='number' value='"
            + escapeHtml(values.brokerPort) + "'>";
        html += "<button type='submit'>Save</button>";
        html += "</form></div></body></html>";
        return html;
    }
```

Change to:

```cpp
        html += "<label>Broker Port</label><input name='broker_port' type='number' value='"
            + escapeHtml(values.brokerPort) + "'>";
        html += "<details><summary>Advanced: Turnout Wiring</summary>";
        for (size_t i = 0; i < values.turnouts.size(); ++i)
        {
            html += renderTurnoutChannel(static_cast<int>(i) + 1, values.turnouts[i]);
        }
        html += "</details>";
        html += "<button type='submit'>Save</button>";
        html += "</form></div></body></html>";
        return html;
    }
```

**3c.** Add the new private static members. The current `private:` section is:

```cpp
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
```

Change to:

```cpp
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

    static std::string renderPinOptions(const std::vector<int>& options, const std::string& selectedValue)
    {
        std::string html;
        html += "<option value=''" + std::string(selectedValue.empty() ? " selected" : "") + ">-- not wired --</option>";
        for (int pin : options)
        {
            std::string value = std::to_string(pin);
            html += "<option value='" + value + "'" + (value == selectedValue ? " selected" : "") + ">" + value + "</option>";
        }
        return html;
    }

    static std::string renderOrientationOptions(const std::string& selectedOrientation)
    {
        std::string effective = selectedOrientation.empty() ? "normal" : selectedOrientation;
        std::string html;
        html += "<option value='normal'" + std::string(effective == "normal" ? " selected" : "") + ">normal</option>";
        html += "<option value='inverted'" + std::string(effective == "inverted" ? " selected" : "") + ">inverted</option>";
        return html;
    }

    static std::string renderTurnoutChannel(int channelNumber, const WebFormTurnoutField& field)
    {
        int index = channelNumber - 1;
        std::string defaultPin = std::to_string(kDefaultPin[index]);
        std::string defaultFeedbackPin = std::to_string(kDefaultFeedbackPin[index]);
        std::string selectedPin = field.pin.empty() ? defaultPin : field.pin;
        std::string selectedFeedbackPin = field.feedbackPin.empty() ? defaultFeedbackPin : field.feedbackPin;
        std::string settleValue = field.settleMs.empty() ? "500" : field.settleMs;
        std::string timeoutValue = field.timeoutMs.empty() ? "4000" : field.timeoutMs;
        std::string prefix = "t" + std::to_string(channelNumber) + "_";

        std::string html;
        html += "<fieldset><legend>Turnout " + std::to_string(channelNumber) + "</legend>";
        html += "<label>Pin</label><select name='" + prefix + "pin'>"
            + renderPinOptions(kOutputPinOptions, selectedPin) + "</select>";
        html += "<label>Feedback Pin</label><select name='" + prefix + "fb'>"
            + renderPinOptions(kFeedbackPinOptions, selectedFeedbackPin) + "</select>";
        html += "<label>Orientation</label><select name='" + prefix + "orientation'>"
            + renderOrientationOptions(field.orientation) + "</select>";
        html += "<label>Settle (ms)</label><input name='" + prefix + "settle' type='number' value='"
            + escapeHtml(settleValue) + "'>";
        html += "<label>Timeout (ms)</label><input name='" + prefix + "timeout' type='number' value='"
            + escapeHtml(timeoutValue) + "'>";
        html += "</fieldset>";
        return html;
    }

    static inline const std::vector<int> kOutputPinOptions{4, 13, 14, 16, 17, 18, 19, 25, 26, 27, 32, 33};
    static inline const std::vector<int> kFeedbackPinOptions{4, 13, 14, 16, 17, 18, 19, 25, 26, 27, 32, 33, 34, 35, 36, 39};
    static inline const std::array<int, 8> kDefaultPin{13, 14, 27, 26, 25, 33, 32, 4};
    static inline const std::array<int, 8> kDefaultFeedbackPin{36, 39, 34, 35, 16, 17, 18, 19};

    static inline const std::string kStyle =
```

**3d.** Add a small amount of CSS for the new elements so the section is legible, not unstyled. The current end of `kStyle` is:

```cpp
        "button{margin-top:24px;width:100%;padding:10px;background:#2563eb;color:#fff;"
        "border:none;border-radius:6px;font-size:1rem;font-weight:600;cursor:pointer;}"
        "button:hover{background:#1d4ed8;}";
```

Change to:

```cpp
        "button{margin-top:24px;width:100%;padding:10px;background:#2563eb;color:#fff;"
        "border:none;border-radius:6px;font-size:1rem;font-weight:600;cursor:pointer;}"
        "button:hover{background:#1d4ed8;}"
        "details{margin-top:20px;}"
        "summary{cursor:pointer;font-size:0.85rem;font-weight:600;color:#374151;}"
        "fieldset{border:1px solid #e5e7eb;border-radius:8px;margin-top:12px;padding:8px 12px 12px;}"
        "legend{font-size:0.8rem;font-weight:600;color:#374151;padding:0 4px;}";
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: `All tests passed`

- [ ] **Step 5: Commit**

Use the `arlo-commits` skill/methodology. This is a `! F` (real new behavior, matching tests, >8 LoC) — e.g. `! F Render turnout wiring fields on the setup page`.

```bash
git add lib/McsCore/src/domain/SetupFormRenderer.h test/test_setup_form_renderer/test_main.cpp
```

Then commit with the composed ACN message.

---

### Task 4: Full verification

**Files:** none (verification only).

**Interfaces:** none — this task just confirms Tasks 1–3 integrate cleanly.

- [ ] **Step 1: Run the full native test suite once (not concurrently with anything else — see `docs/task-status.md`'s note on `.pio/build` races)**

Run: `pio test -e native`
Expected: every test binary `PASSED`, none `FAILED`/`ERRORED`.

- [ ] **Step 2: Build check the ESP32 firmware**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — confirms `CaptivePortalServer.h` (which includes `SetupFormRenderer.h` and uses `WebFormCommissioningAdapter`) still compiles cleanly under the Arduino toolchain, even though neither changed file is itself `#ifdef ARDUINO`-gated.

- [ ] **Step 3: Manual on-device check (not automatable — do this before considering the feature done)**

Flash a board (`pio run -e esp32dev --target upload`), enter wireless setup mode (hold BOOT ~3s, release), connect to `Tortoise-Setup-XXXX`, open the setup page, expand "Advanced: Turnout Wiring", and confirm:
- Each channel's pin/feedback dropdowns show the documented per-channel default selected.
- Changing a value and saving persists it — reopening the page (re-enter setup mode) shows the saved value, not the default.
- Submitting still correctly saves node ID/WiFi/broker as before (regression check on the unchanged path).

No code changes come from this step — it's a pass/fail confirmation. If it fails, that's a bug to fix before Task 4 is considered done, not a step to skip.

- [ ] **Step 4: Update `docs/task-status.md`**

Add a row to the Completed table (after the "MQTT turnout-command activity LED" row and the two MQTT-fix rows already there) noting this feature landed, with the three commit hashes from Tasks 1–3. Follow the existing row format/style in that file exactly.

- [ ] **Step 5: Commit the docs update**

Use the `arlo-commits` skill/methodology. This is a `. d` (docs-only) — e.g. `. d Record turnout wiring setup-page feature in task-status`.

```bash
git add docs/task-status.md
```

Then commit with the composed ACN message.
