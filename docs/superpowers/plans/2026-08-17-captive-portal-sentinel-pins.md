# Captive-Portal Sentinel-Pin Validation Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve backlog #24 — a factory-default board (all 8 turnout slots at sentinel `-1`/`-1` pins) currently fails `NodeConfig::validate()` with ~15 pin-conflict errors, so it cannot complete commissioning through `CaptivePortalServer`'s web form (which only collects `id`/`wifi_ssid`/`wifi_password`/`broker_host`/`broker_port`, not turnout pins).

**Decision (resolved by product owner):** Loosen `NodeConfig::validate()`'s pin-conflict check to treat sentinel pin `-1` as "not yet wired" rather than a conflict, instead of expanding the captive-portal form or giving `factoryDefault()` fabricated non-conflicting pins. This is the smallest change, requires no `CaptivePortalServer`/`WebFormCommissioningAdapter` changes, and keeps the existing division of labor: the captive portal handles network onboarding (id/wifi/broker) and bench serial (`turnout` command) handles per-turnout pin wiring, which a technician does afterward. A board that completes the portal flow with all-sentinel turnout slots will pass `validate()`, boot into `BootMode::Normal`, and simply have 8 non-functional turnout channels until wired via bench serial — this is the same state a board is already in immediately after `NodeConfig::factoryDefault()` combined with only an `id`/`wifi`/`broker` bench-serial session, which already works today.

**Architecture:** Single domain-class change (`NodeConfig::validate()`), no new classes, no port/adapter changes. `-1` is already used as the sentinel "unwired" value in `factoryDefault()` and throughout `TurnoutConfig`; this task teaches the one place that checks for pin conflicts to skip it.

**Tech Stack:** PlatformIO, `native` environment (Catch2). No hardware/build-check needed — this is pure domain logic.

## Global Constraints

- TDD: add the failing test first, then the minimal implementation change.
- `NodeConfig::validate()` must still reject two *real* (non-sentinel) pins claimed by more than one turnout — do not weaken that check for actual pin numbers.
- `NodeConfig::validate()` must still reject an out-of-range node id — unrelated to this change, must keep passing.
- ACN notation for the commit message. Never `--amend`, never `--no-verify`.

---

### Task 1: Treat sentinel pin `-1` as unwired in `NodeConfig::validate()`

**Files:**
- Modify: `lib/McsCore/src/domain/NodeConfig.h`
- Modify: `test/test_node_config/test_main.cpp`

**Interfaces:**
- Consumes: nothing new — `NodeConfig::validate()` already exists with signature `std::vector<ConfigError> validate() const`.
- Produces: nothing consumed by a later task in this plan; this is the whole fix.

- [ ] **Step 1: Write the failing test**

Add this test case to `test/test_node_config/test_main.cpp`, right after the existing `"validate rejects two turnouts claiming the same pin"` test case (around line 132):

```cpp
TEST_CASE("validate does not flag sentinel pin -1 as a conflict")
{
    NodeConfig defaultConfig = NodeConfig::factoryDefault();

    std::vector<ConfigError> errors = defaultConfig.validate();

    REQUIRE(errors.size() == 1);
    REQUIRE(errors[0] == ConfigError{"Node id out of range (must be 1-16)"});
}
```

`NodeConfig::factoryDefault()` returns a config with node id `0` (invalid, 1 expected error) and all 8 turnout slots at pins `-1`/`-1` (currently 15 additional false-positive pin-conflict errors: 16 sentinel pin occurrences, first one seen doesn't error, remaining 15 each match the already-seen `-1`).

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_node_config`
Expected: FAIL — `errors.size()` is `16`, not `1` (the new test case fails; all prior test cases in this file still pass).

- [ ] **Step 3: Implement the minimal fix**

In `lib/McsCore/src/domain/NodeConfig.h`, in `validate()`, change:

```cpp
        std::vector<int> seenPins;
        for (const auto& turnout : turnouts_)
        {
            for (int pin : {turnout.outputPin(), turnout.feedbackPin()})
            {
                bool alreadySeen = std::find(seenPins.begin(), seenPins.end(), pin) != seenPins.end();
                if (alreadySeen)
                {
                    errors.push_back(ConfigError{"Pin " + std::to_string(pin) + " used by more than one turnout"});
                }
                seenPins.push_back(pin);
            }
        }
```

to:

```cpp
        std::vector<int> seenPins;
        for (const auto& turnout : turnouts_)
        {
            for (int pin : {turnout.outputPin(), turnout.feedbackPin()})
            {
                if (pin == -1)
                {
                    continue;
                }

                bool alreadySeen = std::find(seenPins.begin(), seenPins.end(), pin) != seenPins.end();
                if (alreadySeen)
                {
                    errors.push_back(ConfigError{"Pin " + std::to_string(pin) + " used by more than one turnout"});
                }
                seenPins.push_back(pin);
            }
        }
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_node_config`
Expected: PASS — all test cases in the file pass, including the new one and the pre-existing `"validate rejects two turnouts claiming the same pin"` (uses real pins `100`/`201`, unaffected) and `"factoryDefault fails validate, since it still needs commissioning"` (still fails validate, now solely due to node id `0`).

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: All test binaries pass (no regressions elsewhere — nothing else calls `validate()`'s pin-conflict path with `-1`).

- [ ] **Step 6: Commit**

```bash
git add lib/McsCore/src/domain/NodeConfig.h test/test_node_config/test_main.cpp
git commit -m "$(cat <<'EOF'
^ B Treat sentinel pin -1 as unwired in NodeConfig::validate

Factory-default boards (all 8 turnout slots at -1/-1) previously
failed validate() with ~15 false-positive pin-conflict errors,
blocking captive-portal commissioning (which only collects
id/wifi/broker, not turnout pins). -1 already means "not yet wired"
everywhere else in TurnoutConfig/factoryDefault - validate() is the
one place that didn't know that. Real pin conflicts are still
rejected; only the sentinel value is exempted.

EOF
)"
```

---

### Task 2: Update `docs/task-status.md`

**Files:**
- Modify: `docs/task-status.md`

**Interfaces:**
- Consumes: the commit hash from Task 1 (look it up with `git log --oneline` — do not guess).
- Produces: nothing (docs only).

- [ ] **Step 1: Add a Completed row**

Add a row to the Completed table (after the "Wireless setup mode boot logic" row), citing the real commit hash from Task 1:

```markdown
| Captive-portal factory-default turnout-fields fix (Backlog #24) | ✅ Done | Commit `<hash>`. `NodeConfig::validate()` now treats sentinel pin `-1` as "not yet wired" rather than a conflict, so a factory-default board (all 8 turnout slots at `-1`/`-1`) passes validation after only `id`/`wifi`/`broker` are set via the captive portal. Turnout pin wiring is still done afterward via bench serial's `turnout` command — no `CaptivePortalServer`/`WebFormCommissioningAdapter` changes needed. Resolved via product decision: loosen `validate()`'s sentinel handling rather than expand the portal form or fabricate non-conflicting `factoryDefault()` pins. |
```

- [ ] **Step 2: Remove the now-resolved captive-portal debt bullet**

In "Known scaffolding debt", remove the bullet starting `"CaptivePortalServer's served form (task #19) collects id, wifi_ssid, ..."` (the one ending with `"...requires a product decision to resolve..."` / `"...Deferred rather than fixed in this branch..."`). It's resolved.

- [ ] **Step 3: Commit**

```bash
git add docs/task-status.md
git commit -m "$(cat <<'EOF'
. d Mark captive-portal sentinel-pin fix complete in task-status.md

EOF
)"
```
