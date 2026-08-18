# Field Identification + Collision Guard Wiring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve backlog #25 — wire the already-built, already-tested field-identification classes (`ButtonIdentifyRequestTrigger`, `BlinkOutIdentifier`, `NodeIdCollisionGuard`, `MqttNodePresenceReporter`) into `ControllerNode`/`src/main.cpp` for real, per `docs/software-class-list.md`'s "Field Identification: Blink-Out" and "Duplicate Node ID Detection" sections: a short BOOT press during normal operation blinks the status LED the node's id N times; on boot, before fully starting, the node announces retained MQTT presence and refuses to fully come online (no turnout command handling) if another session already claims its id, blinking a distinct steady error pattern instead.

**Decision (resolved by product owner):** Status LED is **GPIO 2** (ESP32-WROOM-32 dev-board onboard LED pin).

**Architecture:** Two design gaps block a pure "just wire it" task, resolved here as part of the wiring work rather than deferred:

1. **Single-callback bottleneck:** `PubSubClient` (the underlying MQTT library) supports exactly one registered callback. Today `MqttCommandSource`'s constructor claims it exclusively via `link.raw().setCallback(...)`. Wiring presence-topic observation (a second, independent subscription) would silently clobber that registration if done the same way. Fixed by giving `MqttLink` itself a small topic-keyed dispatch table (`MqttTopicRouter`, a new pure domain class) and a `subscribe(topic, handler)` method — the one place that owns the raw `PubSubClient` resource is the one place that owns its one callback slot. `MqttCommandSource` is refactored to register through `MqttLink::subscribe` instead of touching `raw()` directly; this is a zero-behavior-change refactor from callers' perspective.
2. **Distinct blink pattern:** the design doc requires the collision-error blink to look different from the per-id identify blink. `BlinkOutIdentifier` is `NodeId`-shaped and wrong for this (a collision means the id *can't* be trusted). A new tiny pure domain class, `SteadyBlinker` (fixed-rate square wave), covers it — same size and testing style as `BlinkOutIdentifier`.

No new ports are needed — `NodePresenceReporter`/`MqttNodePresenceReporter`, `IdentifyRequestTrigger`/`ButtonIdentifyRequestTrigger`, and `NodeIdCollisionGuard` already exist and are unchanged by this plan (see `docs/task-status.md`'s "Field identification + duplicate node ID detection" row for their original commits).

**Tech Stack:** PlatformIO, `native` environment (Catch2) for new pure domain classes, `esp32dev` environment (build-check only) for adapter/composition-root changes — `MqttLink`, `MqttCommandSource`, `ControllerNode`, and `src/main.cpp` are all `#ifdef ARDUINO`-gated or have no native equivalent, consistent with every other ESP32-only class in this codebase.

## Global Constraints

- TDD for every new pure domain class (`MqttTopicRouter`, `SteadyBlinker`, `Deadline::armed()`): failing native test first, then minimal implementation.
- `src/main.cpp` is the composition root only — no business logic. All blink-pattern math belongs in `BlinkOutIdentifier`/`SteadyBlinker`/`Deadline`, not inline in `main.cpp`.
- No `delay()` in `loop()`. The one bounded spin-wait this plan adds (`ControllerNode::begin()`'s presence-check window) runs once during `begin()`, not in `loop()` — same precedent as `src/main.cpp`'s existing boot-window detection.
- No dynamic allocation after boot — new objects are function-local statics inside `setup()`, same pattern as the existing code.
- Hardware-touching adapter constructors (anything calling `pinMode`/NVS/etc.) must stay function-local-static inside `setup()`, never file-scope globals — file-scope statics run before `initArduino()`, which silently breaks NVS. Plain domain value objects (`Deadline`, `Instant`, `bool`) have no hardware side effects and are safe as ordinary file-scope statics.
- ACN notation for every commit message. Never `--amend`, never `--no-verify`.
- Ordinary hand-edited F/B changes over 8 LoC, or without a matching native test, are `!` risk, not `^` — do not under-classify. `MqttLink`/`MqttCommandSource`/`ControllerNode`/`src/main.cpp` changes have no native test available (ARDUINO-gated/no native equivalent), so they are `!` regardless of size.

---

### Task 1: `MqttTopicRouter` domain class

**Files:**
- Create: `lib/McsCore/src/domain/MqttTopicRouter.h`
- Test: `test/test_mqtt_topic_router/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `MqttTopicRouter` (used by Task 2's `MqttLink`) — `using Handler = std::function<void(const std::string&)>`, `void on(const std::string& topic, Handler handler)`, `void dispatch(const std::string& topic, const std::string& payload) const`.

- [ ] **Step 1: Write the failing tests**

Create `test/test_mqtt_topic_router/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/MqttTopicRouter.h"

TEST_CASE("dispatch calls the handler registered for the matching topic")
{
    MqttTopicRouter router;
    std::string received;
    router.on("a/topic", [&received](const std::string& payload) { received = payload; });

    router.dispatch("a/topic", "hello");

    REQUIRE(received == "hello");
}

TEST_CASE("dispatch does nothing for an unregistered topic")
{
    MqttTopicRouter router;
    bool called = false;
    router.on("a/topic", [&called](const std::string&) { called = true; });

    router.dispatch("other/topic", "hello");

    REQUIRE_FALSE(called);
}

TEST_CASE("dispatch only calls the handler for the matching topic, not others")
{
    MqttTopicRouter router;
    std::string firstReceived;
    std::string secondReceived;
    router.on("topic/one", [&firstReceived](const std::string& payload) { firstReceived = payload; });
    router.on("topic/two", [&secondReceived](const std::string& payload) { secondReceived = payload; });

    router.dispatch("topic/two", "world");

    REQUIRE(firstReceived.empty());
    REQUIRE(secondReceived == "world");
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_mqtt_topic_router`
Expected: FAIL to compile — `domain/MqttTopicRouter.h` doesn't exist yet.

- [ ] **Step 3: Write the implementation**

Create `lib/McsCore/src/domain/MqttTopicRouter.h`:

```cpp
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

class MqttTopicRouter
{
public:
    using Handler = std::function<void(const std::string&)>;

    void on(const std::string& topic, Handler handler)
    {
        handlers_.emplace_back(topic, std::move(handler));
    }

    void dispatch(const std::string& topic, const std::string& payload) const
    {
        for (const auto& entry : handlers_)
        {
            if (entry.first == topic)
            {
                entry.second(payload);
            }
        }
    }

private:
    std::vector<std::pair<std::string, Handler>> handlers_;
};
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_mqtt_topic_router`
Expected: PASS, all 3 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/domain/MqttTopicRouter.h test/test_mqtt_topic_router/test_main.cpp
git commit -m "$(cat <<'EOF'
! F Add MqttTopicRouter domain class

PubSubClient supports exactly one registered callback, but the next
task needs to observe a second topic (node presence) alongside
MqttCommandSource's existing turnout-command topics without either
clobbering the other. MqttTopicRouter is a pure, exact-match
topic-to-handler table that MqttLink will own as the single
registrant of that one callback slot. Native-tested in isolation
from PubSubClient - no ARDUINO dependency.

EOF
)"
```

---

### Task 2: Wire `MqttTopicRouter` into `MqttLink`

**Files:**
- Modify: `lib/McsCore/src/adapters/MqttLink.h`

**Interfaces:**
- Consumes: `MqttTopicRouter` (Task 1).
- Produces: `MqttLink::subscribe(const std::string& topic, MqttTopicRouter::Handler handler)` — used by Task 3 (`MqttCommandSource`) and Task 5 (`ControllerNode`'s presence subscription).

- [ ] **Step 1: Make the change**

In `lib/McsCore/src/adapters/MqttLink.h`, add the include and register the router's `dispatch` as the one `PubSubClient` callback in the constructor, and add a `subscribe` method:

```cpp
#pragma once

#ifdef ARDUINO

#include <WiFiClient.h>
#include <PubSubClient.h>

#include <string>
#include <utility>

#include "ports/Clock.h"
#include "domain/Instant.h"
#include "domain/Duration.h"
#include "domain/BrokerAddress.h"
#include "domain/MqttTopicRouter.h"

class MqttLink
{
public:
    MqttLink(Clock& clock, Duration retryInterval, std::string clientId, std::string willTopic, std::string willMessage)
        : clock_(clock),
          retryInterval_(retryInterval),
          clientId_(std::move(clientId)),
          willTopic_(std::move(willTopic)),
          willMessage_(std::move(willMessage)),
          client_(wifiClient_),
          lastAttempt_(Instant(0))
    {
        client_.setCallback([this](char* topic, byte* payload, unsigned int length) {
            std::string text(reinterpret_cast<char*>(payload), length);
            router_.dispatch(topic, text);
        });
    }

    void begin(const BrokerAddress& broker)
    {
        client_.setServer(broker.host().c_str(), broker.port());
        connect();
    }

    void poll()
    {
        if (client_.connected())
        {
            client_.loop();
            return;
        }

        if (clock_.now() - lastAttempt_ >= retryInterval_)
        {
            connect();
        }
    }

    // Not const: PubSubClient::connected() isn't const-qualified.
    bool connected()
    {
        return client_.connected();
    }

    // The one place PubSubClient::setCallback is ever called is this
    // class's constructor above - every subscriber routes through here so
    // no caller can silently clobber another's callback registration.
    void subscribe(const std::string& topic, MqttTopicRouter::Handler handler)
    {
        router_.on(topic, std::move(handler));
        client_.subscribe(topic.c_str());
    }

    PubSubClient& raw()
    {
        return client_;
    }

private:
    void connect()
    {
        client_.connect(clientId_.c_str(), willTopic_.c_str(), 1, true, willMessage_.c_str());
        lastAttempt_ = clock_.now();
    }

    Clock& clock_;
    Duration retryInterval_;
    std::string clientId_;
    std::string willTopic_;
    std::string willMessage_;
    WiFiClient wifiClient_;
    PubSubClient client_;
    Instant lastAttempt_;
    MqttTopicRouter router_;
};

#endif
```

`raw()` is kept — `MqttPositionReporter` still calls `link_.raw().publish(...)` for outbound publishing, which doesn't touch the callback and is out of this plan's scope.

- [ ] **Step 2: Build-check**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`. (`MqttCommandSource` still compiles against the old `raw()`-based API at this point — Task 3 migrates it. `MqttLink` itself has no other callers of `subscribe` yet.)

- [ ] **Step 3: Commit**

```bash
git add lib/McsCore/src/adapters/MqttLink.h
git commit -m "$(cat <<'EOF'
! F Add MqttLink::subscribe backed by MqttTopicRouter

MqttLink now owns the PubSubClient's one callback slot itself,
dispatching by topic through MqttTopicRouter, instead of letting
whichever caller happens to construct first grab setCallback()
exclusively. Existing raw()-based callers (MqttCommandSource,
MqttPositionReporter) are unaffected until migrated. Build-check
verified only (pio run -e esp32dev) - PubSubClient has no native
equivalent.

EOF
)"
```

---

### Task 3: Migrate `MqttCommandSource` onto `MqttLink::subscribe`

**Files:**
- Modify: `lib/McsCore/src/adapters/MqttCommandSource.h`

**Interfaces:**
- Consumes: `MqttLink::subscribe` (Task 2).
- Produces: `MqttCommandSource::subscribeAll(int nodeId)` — same public signature as before, no caller changes needed.

- [ ] **Step 1: Make the change**

Replace the full contents of `lib/McsCore/src/adapters/MqttCommandSource.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <optional>
#include <string>

#include "adapters/MqttLink.h"
#include "domain/TopicScheme.h"
#include "domain/PayloadCodec.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutRegistry.h"
#include "ports/TurnoutCommandSink.h"

class MqttCommandSource
{
public:
    MqttCommandSource(MqttLink& link, TurnoutCommandSink& sink)
        : link_(link), sink_(sink)
    {
    }

    void subscribeAll(int nodeId)
    {
        for (int channel = 1; channel <= TurnoutRegistry::TurnoutsPerNode; ++channel)
        {
            TurnoutId id(nodeId * 100 + channel);
            std::string topic = TopicScheme::topicFor(id);
            link_.subscribe(topic, [this, id](const std::string& payload) {
                handle(id, payload);
            });
        }
    }

private:
    void handle(TurnoutId id, const std::string& payload)
    {
        std::optional<TurnoutPosition> position = PayloadCodec::decode(payload);
        if (!position.has_value())
        {
            return;
        }

        sink_.command(id, *position);
    }

    MqttLink& link_;
    TurnoutCommandSink& sink_;
};

#endif
```

This drops the `Arduino.h`/`PubSubClient.h` includes (no longer touches `raw()` or `byte*` directly) and the `TopicScheme::parse` round-trip — the topic is already known at subscribe time via the closure, so there's nothing left to parse back out of the incoming message. `TopicScheme::parse` itself is untouched and still used/tested elsewhere.

- [ ] **Step 2: Build-check**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`. `ControllerNode`'s existing `commandSource_.subscribeAll(config_.id().value())` call in `begin()` is unchanged and needs no edits — same public method, same external behavior.

- [ ] **Step 3: Commit**

```bash
git add lib/McsCore/src/adapters/MqttCommandSource.h
git commit -m "$(cat <<'EOF'
! r Route MqttCommandSource through MqttLink::subscribe

No external behavior change - subscribeAll(nodeId) still subscribes
the same 8 topics and calls sink_.command() the same way. Internally,
callback registration now goes through MqttLink's shared router
instead of claiming PubSubClient's one callback slot directly, and
the per-message TopicScheme::parse round-trip is no longer needed
since the topic is already known via the subscribe-time closure.
Build-check verified only (pio run -e esp32dev) - no native
equivalent for PubSubClient-dependent code.

EOF
)"
```

---

### Task 4: `SteadyBlinker` domain class

**Files:**
- Create: `lib/McsCore/src/domain/SteadyBlinker.h`
- Test: `test/test_steady_blinker/test_main.cpp`

**Interfaces:**
- Consumes: `Duration`, `Level` (existing).
- Produces: `SteadyBlinker` (used by Task 6's `src/main.cpp`) — `explicit SteadyBlinker(Duration halfPeriod)`, `Level levelAt(Duration elapsed) const`.

- [ ] **Step 1: Write the failing tests**

Create `test/test_steady_blinker/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/SteadyBlinker.h"
#include "domain/Duration.h"
#include "domain/Level.h"

TEST_CASE("SteadyBlinker is High for the first half of each period")
{
    SteadyBlinker blinker(Duration(250));

    REQUIRE(blinker.levelAt(Duration(0)) == Level::High);
    REQUIRE(blinker.levelAt(Duration(100)) == Level::High);
    REQUIRE(blinker.levelAt(Duration(249)) == Level::High);
}

TEST_CASE("SteadyBlinker is Low for the second half of each period")
{
    SteadyBlinker blinker(Duration(250));

    REQUIRE(blinker.levelAt(Duration(250)) == Level::Low);
    REQUIRE(blinker.levelAt(Duration(400)) == Level::Low);
    REQUIRE(blinker.levelAt(Duration(499)) == Level::Low);
}

TEST_CASE("SteadyBlinker repeats indefinitely")
{
    SteadyBlinker blinker(Duration(250));

    REQUIRE(blinker.levelAt(Duration(500)) == Level::High);
    REQUIRE(blinker.levelAt(Duration(750)) == Level::Low);
    REQUIRE(blinker.levelAt(Duration(1000)) == Level::High);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_steady_blinker`
Expected: FAIL to compile — `domain/SteadyBlinker.h` doesn't exist yet.

- [ ] **Step 3: Write the implementation**

Create `lib/McsCore/src/domain/SteadyBlinker.h`:

```cpp
#pragma once

#include "domain/Duration.h"
#include "domain/Level.h"

// Fixed-rate on/off square wave, visually distinct from BlinkOutIdentifier's
// per-id blink-count pattern. Used for the collision-error indicator, where
// the node's id can't be trusted (that's the whole problem).
class SteadyBlinker
{
public:
    explicit SteadyBlinker(Duration halfPeriod) : halfPeriod_(halfPeriod)
    {
    }

    Level levelAt(Duration elapsed) const
    {
        unsigned long half = halfPeriod_.milliseconds();
        unsigned long t = elapsed.milliseconds() % (half * 2);
        return t < half ? Level::High : Level::Low;
    }

private:
    Duration halfPeriod_;
};
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_steady_blinker`
Expected: PASS, all 3 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/domain/SteadyBlinker.h test/test_steady_blinker/test_main.cpp
git commit -m "$(cat <<'EOF'
! F Add SteadyBlinker domain class

The collision-error LED pattern must look distinct from
BlinkOutIdentifier's per-id blink-count pattern per
docs/software-class-list.md's "Duplicate Node ID Detection" section.
BlinkOutIdentifier is NodeId-shaped and wrong here - a collision
means the id can't be trusted. SteadyBlinker is a pure fixed-rate
square wave, same size and testing style as BlinkOutIdentifier.

EOF
)"
```

---

### Task 5: Wire presence announce + collision guard into `ControllerNode`

**Files:**
- Modify: `lib/McsCore/src/adapters/ControllerNode.h`

**Interfaces:**
- Consumes: `MqttLink::subscribe` (Task 2), `MqttNodePresenceReporter` (existing, `lib/McsCore/src/adapters/MqttNodePresenceReporter.h` — `explicit MqttNodePresenceReporter(MqttLink&)`, `void announce(NodeId)`), `NodeIdCollisionGuard`/`PresenceVerdict` (existing, `lib/McsCore/src/domain/NodeIdCollisionGuard.h` — `explicit NodeIdCollisionGuard(NodeId selfId)`, `PresenceVerdict evaluate(NodeId observedId, bool hasAnnouncedSelf) const`).
- Produces: `ControllerNode::blocked() const` — used by Task 6's `src/main.cpp` to decide whether to drive the collision-error blink instead of the identify blink.

- [ ] **Step 1: Make the change**

In `lib/McsCore/src/adapters/ControllerNode.h`:

Add includes, alongside the existing adapter includes:

```cpp
#include "adapters/MqttNodePresenceReporter.h"
#include "domain/NodeIdCollisionGuard.h"
```

Add a presence-check window constant, alongside the existing `kFeedbackDebounceMs`/`kLinkRetryMs`:

```cpp
    static constexpr unsigned long kPresenceCheckWindowMs = 500;
```

Replace `begin()`:

```cpp
    void begin()
    {
        wifiLink_.begin(config_.wifi());
        mqttLink_.begin(config_.broker());

        std::string presenceTopic = "node/" + std::to_string(config_.id().value()) + "/status";
        mqttLink_.subscribe(presenceTopic, [this](const std::string& payload) {
            if (payload != "online")
            {
                return;
            }

            if (collisionGuard_.evaluate(config_.id(), announcedSelf_) == PresenceVerdict::Collision)
            {
                blocked_ = true;
            }
        });

        // Bounded clock-based spin so a pre-existing retained presence
        // message for our id (from a different node/session already
        // claiming it) has a chance to arrive before we decide whether to
        // announce ourselves. Not a blocking delay() - runs once during
        // begin(), same pattern as src/main.cpp's boot-window detection.
        Instant start = clock_.now();
        while (clock_.now() - start < Duration(kPresenceCheckWindowMs))
        {
            mqttLink_.poll();
        }

        if (!blocked_)
        {
            presenceReporter_.announce(config_.id());
            announcedSelf_ = true;
            commandSource_.subscribeAll(config_.id().value());
        }
    }

    void tick()
    {
        wifiLink_.poll();
        mqttLink_.poll();
        registry_.tick(clock_.now());
    }

    // True if a different session already claimed this node's id (retained
    // "online" observed on our own presence topic before we announced
    // ourselves). While true, turnout commands were never subscribed to -
    // src/main.cpp drives a distinct error blink pattern instead of the
    // normal identify blink.
    bool blocked() const
    {
        return blocked_;
    }
```

(`tick()` is unchanged — shown above only for placement context; do not alter it.)

Add members after the existing `MqttCommandSource commandSource_{mqttLink_, registry_};`:

```cpp
    MqttNodePresenceReporter presenceReporter_{mqttLink_};
    NodeIdCollisionGuard collisionGuard_{config_.id()};
    bool announcedSelf_ = false;
    bool blocked_ = false;
```

- [ ] **Step 2: Build-check**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 3: Commit**

```bash
git add lib/McsCore/src/adapters/ControllerNode.h
git commit -m "$(cat <<'EOF'
! F Wire presence announce and collision guard into ControllerNode

Per docs/software-class-list.md's "Duplicate Node ID Detection":
begin() now subscribes to this node's own presence topic and gives
any pre-existing retained "online" message a bounded window to
arrive before deciding whether to announce itself. If another
session already claims the id (NodeIdCollisionGuard evaluates
Collision), the node skips both the presence announce and turnout
command subscription - it stays connected to WiFi/MQTT but never
becomes commandable. blocked() exposes that state for src/main.cpp
to drive a distinct LED pattern. Build-check verified only (pio run
-e esp32dev) - no native equivalent for PubSubClient-dependent code.

EOF
)"
```

---

### Task 6: Add `Deadline::armed()` and wire identify-blink/collision-blink into `src/main.cpp`

**Files:**
- Modify: `lib/McsCore/src/domain/Deadline.h`
- Modify: `test/test_deadline/test_main.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `Deadline` (existing, extended here), `ButtonIdentifyRequestTrigger` (existing, `lib/McsCore/src/adapters/ButtonIdentifyRequestTrigger.h` — `ButtonIdentifyRequestTrigger(DigitalInput&, Duration minPressDuration, Duration maxPressDuration)`, `void poll(Instant now)`, `bool requested() const`), `BlinkOutIdentifier` (existing, `lib/McsCore/src/domain/BlinkOutIdentifier.h` — `BlinkOutIdentifier(NodeId, Duration onDuration, Duration offDuration, Duration pauseDuration)`, `Level levelAt(Duration elapsed) const`), `SteadyBlinker` (Task 4), `ControllerNode::blocked()` (Task 5).
- Produces: nothing consumed by a later task — composition root, end of the dependency chain.

- [ ] **Step 1: Write the failing test for `Deadline::armed()`**

Add this test case to `test/test_deadline/test_main.cpp`, after the existing `"A Deadline that has never been armed is not expired"` test case:

```cpp
TEST_CASE("armed reflects whether the Deadline has been armed and not disarmed")
{
    Deadline deadline;
    REQUIRE_FALSE(deadline.armed());

    deadline.arm(Instant(100), Duration(50));
    REQUIRE(deadline.armed());

    deadline.disarm();
    REQUIRE_FALSE(deadline.armed());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_deadline`
Expected: FAIL to compile — `Deadline::armed()` doesn't exist yet.

- [ ] **Step 3: Implement `Deadline::armed()`**

In `lib/McsCore/src/domain/Deadline.h`, add a public accessor:

```cpp
    bool armed() const
    {
        return armed_;
    }
```

Place it after the existing `expired(Instant now) const` method.

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_deadline`
Expected: PASS, all test cases including the new one.

- [ ] **Step 5: Commit the `Deadline::armed()` addition**

```bash
git add lib/McsCore/src/domain/Deadline.h test/test_deadline/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add Deadline::armed accessor

src/main.cpp needs to distinguish "never triggered" from "triggered
and still active" from "triggered and expired" to drive the
identify-blink window without duplicating that state itself. Purely
additive - exposes existing private state, no behavior change to
arm/expired/disarm.

EOF
)"
```

- [ ] **Step 6: Read the current file**

Read `src/main.cpp` in full and confirm its exact current content matches the wireless-setup-mode-wiring plan's Task 1 output (a `detectWirelessSetupRequest()` free function, `node`/`commissioningAdapter`/`captivePortal`/`webFormAdapter` static pointers, `setup()`/`loop()` branching on `BootMode`). If it has drifted from this description, stop and report rather than guessing.

- [ ] **Step 7: Make the change**

Replace the full contents of `src/main.cpp`:

```cpp
#include <Arduino.h>

#include "adapters/ControllerNode.h"
#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ButtonSetupModeTrigger.h"
#include "adapters/ButtonIdentifyRequestTrigger.h"
#include "adapters/EspDigitalInput.h"
#include "adapters/EspDigitalOutput.h"
#include "adapters/ArduinoClock.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "adapters/CaptivePortalServer.h"
#include "domain/CommissioningSession.h"
#include "domain/NodeConfig.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/BlinkOutIdentifier.h"
#include "domain/SteadyBlinker.h"
#include "domain/Deadline.h"
#include "domain/Level.h"
#include "domain/Duration.h"
#include "domain/Instant.h"

namespace
{
// How long BOOT must be held through power-on to enter wireless setup mode.
// This is an unavoidable fixed delay on every boot, not just when BOOT is
// held - see this plan's Global Constraints for why.
constexpr unsigned long kBootWindowMs = 2000;

// Short-press window for the runtime identify-blink trigger (field
// identification), distinguished from the setup-mode hold above by a
// shorter max duration - reuses the same physical BOOT pin, per
// docs/software-class-list.md's "Field Identification: Blink-Out" design.
constexpr unsigned long kIdentifyMinPressMs = 50;
constexpr unsigned long kIdentifyMaxPressMs = 1500;

// How long the identify-blink sequence stays active after a qualifying
// short press, before the status LED goes dark again.
constexpr unsigned long kIdentifyActiveMs = 5000;

// Blink half-period for the distinct collision-error pattern - steady fast
// blink, visually different from the per-id identify pattern.
constexpr unsigned long kCollisionBlinkHalfPeriodMs = 250;

// GPIO 0 is the BOOT button on ESP32-WROOM-32 dev boards - active-low, tied
// high via internal pull-up when not pressed. Reading it here in setup() is
// well after the ROM bootloader's own strapping-pin decision has resolved.
bool detectWirelessSetupRequest(EspDigitalInput& bootPin)
{
    static ArduinoClock bootClock;
    static ButtonSetupModeTrigger trigger(bootPin, Duration(kBootWindowMs));

    Instant start = bootClock.now();
    trigger.poll(start);
    while ((bootClock.now() - start) < Duration(kBootWindowMs))
    {
        trigger.poll(bootClock.now());
    }

    return trigger.requested();
}
}

// ControllerNode is constructed here (function-local static, not file-scope)
// because its constructor performs real NVS/GPIO work via NvsConfigStore and
// the Esp*Output/Input adapters. A file-scope static would run before
// app_main() calls initArduino() (see cores/esp32/main.cpp), which is the
// only place nvs_flash_init() runs (esp32-hal-misc.c) - constructing before
// that silently drops all saved NVS config to factory defaults. setup() only
// runs after initArduino(), so this ordering is safe. Every other
// hardware-touching adapter below follows the same rule; plain domain value
// objects (Deadline, Instant, bool) don't touch hardware and are safe as
// ordinary file-scope statics.
static ControllerNode* node = nullptr;
static SerialCommissioningAdapter* commissioningAdapter = nullptr;
static CaptivePortalServer* captivePortal = nullptr;
static WebFormCommissioningAdapter* webFormAdapter = nullptr;
static EspDigitalOutput* statusLed = nullptr;
static ArduinoClock* blinkClock = nullptr;
static ButtonIdentifyRequestTrigger* identifyTrigger = nullptr;
static BlinkOutIdentifier* blinkIdentifier = nullptr;
static SteadyBlinker* collisionBlinker = nullptr;
static Deadline identifyDeadline;
static Instant identifyStart(0);

void setup()
{
    // Bench serial commissioning runs in parallel with every other mode
    // below, regardless of config validity or wireless setup - it's a
    // distinct physical channel (UART) from both the turnout GPIO/MQTT
    // graph and the wireless setup AP/HTTP server.
    static NvsConfigStore commissioningStore;
    static CommissioningSession commissioningSession(commissioningStore);
    static EspUartPort uart(115200);
    static SerialCommissioningAdapter adapter(uart, commissioningSession);
    commissioningAdapter = &adapter;

    // Shared with the runtime identify-blink trigger below (Normal mode
    // only) - same physical BOOT pin, distinguished by press duration, per
    // docs/software-class-list.md.
    static EspDigitalInput bootPin(0, true);
    bool wirelessSetupRequested = detectWirelessSetupRequest(bootPin);
    NodeConfig config = commissioningStore.load();
    BootMode mode = BootModeSelector::select(config, wirelessSetupRequested);

    if (mode == BootMode::WirelessSetup)
    {
        // Shares the same commissioningSession as bench serial - either
        // channel edits the same draft NodeConfig, both save() to the same
        // NvsConfigStore. ControllerNode is not constructed in this mode:
        // the AP takes over WiFi instead of connecting to the home network.
        static EspDeviceIdentity deviceIdentity;
        static WebFormCommissioningAdapter formAdapter(commissioningSession);
        webFormAdapter = &formAdapter;
        static CaptivePortalServer portal(formAdapter, deviceIdentity.mac());
        captivePortal = &portal;
        captivePortal->begin();
    }
    else if (mode == BootMode::Normal)
    {
        static ControllerNode instance;
        node = &instance;
        node->begin();

        // Field identification (short-press BOOT blinks the node's id) and
        // the distinct collision-error pattern (steady fast blink, driven
        // instead whenever ControllerNode::blocked() is true) share one
        // physical status LED - only meaningful once a node has an actual
        // id, so these are only constructed in Normal mode.
        static EspDigitalOutput led(2);
        statusLed = &led;
        static ArduinoClock ledClock;
        blinkClock = &ledClock;
        static ButtonIdentifyRequestTrigger trigger(bootPin, Duration(kIdentifyMinPressMs), Duration(kIdentifyMaxPressMs));
        identifyTrigger = &trigger;
        static BlinkOutIdentifier identifier(config.id(), Duration(200), Duration(200), Duration(1000));
        blinkIdentifier = &identifier;
        static SteadyBlinker errorBlinker(Duration(kCollisionBlinkHalfPeriodMs));
        collisionBlinker = &errorBlinker;
    }

    // BootMode::NeedsCommissioning: neither node nor captivePortal is
    // constructed - loop() below only runs the always-on serial channel.
}

void loop()
{
    if (node != nullptr)
    {
        node->tick();

        Instant now = blinkClock->now();

        if (node->blocked())
        {
            statusLed->write(collisionBlinker->levelAt(now - Instant(0)));
        }
        else
        {
            identifyTrigger->poll(now);
            if (identifyTrigger->requested())
            {
                identifyDeadline.arm(now, Duration(kIdentifyActiveMs));
                identifyStart = now;
            }

            bool identifying = identifyDeadline.armed() && !identifyDeadline.expired(now);
            statusLed->write(identifying ? blinkIdentifier->levelAt(now - identifyStart) : Level::Low);
        }
    }

    if (captivePortal != nullptr)
    {
        captivePortal->poll();
        if (webFormAdapter->rebootRequested())
        {
            ESP.restart();
        }
    }

    commissioningAdapter->poll();
    if (commissioningAdapter->rebootRequested())
    {
        ESP.restart();
    }
}
```

- [ ] **Step 8: Build-check**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`. This is the real, permanent target file — there is no revert step.

- [ ] **Step 9: Commit**

```bash
git add src/main.cpp
git commit -m "$(cat <<'EOF'
! F Wire field identification and collision-blink into main.cpp

A short BOOT press during Normal mode now blinks the status LED
(GPIO 2) the node's id N times for 5 seconds, via
ButtonIdentifyRequestTrigger/BlinkOutIdentifier - reusing the same
physical BOOT pin as the wireless-setup hold trigger, distinguished
by press duration. Whenever ControllerNode::blocked() is true (a
different session already claims this node's id), the same LED
instead drives SteadyBlinker's distinct steady-fast error pattern.
Both are Normal-mode-only, since identification only makes sense
once a node has an actual id. Build-check verified only (pio run -e
esp32dev) - main.cpp has no native equivalent.

EOF
)"
```

---

### Task 7: Update `docs/task-status.md`

**Files:**
- Modify: `docs/task-status.md`

**Interfaces:**
- Consumes: the real commit hashes from Tasks 1–6 (look them up with `git log --oneline` — do not guess).
- Produces: nothing (docs only).

- [ ] **Step 1: Add a Completed row**

Add a row to the Completed table (after the "Captive-portal factory-default turnout-fields fix" row, if that plan landed first — otherwise after "Wireless setup mode boot logic"), citing the real commit hashes from Tasks 1–6:

```markdown
| Field identification + collision guard wiring (Backlog #25) | ✅ Done | Commits `<hash1>` (`MqttTopicRouter`), `<hash2>` (`MqttLink::subscribe`), `<hash3>` (migrate `MqttCommandSource`), `<hash4>` (`SteadyBlinker`), `<hash5>` (`ControllerNode` presence/collision wiring), `<hash6>` (`Deadline::armed()` + `src/main.cpp` wiring). Status LED is GPIO 2. `MqttLink` now owns `PubSubClient`'s one callback slot via a new `MqttTopicRouter`, dispatching by exact topic match, so the presence-topic subscription and the 8 turnout-command subscriptions can coexist. A short BOOT press in `BootMode::Normal` blinks the node's id via `BlinkOutIdentifier` for 5 seconds; `ControllerNode::begin()` gives a pre-existing retained presence message a bounded 500ms window to arrive before announcing itself, and if `NodeIdCollisionGuard` reports a collision, skips both the announce and turnout-command subscription, exposing `blocked()` so `main.cpp` drives `SteadyBlinker`'s distinct steady-fast pattern on the same LED instead. |
```

- [ ] **Step 2: Remove the now-resolved wiring debt bullet**

In "Known scaffolding debt", remove the bullet starting `"ButtonIdentifyRequestTrigger, BlinkOutIdentifier, NodeIdCollisionGuard, and MqttNodePresenceReporter (task #20) are not yet wired..."`. It's resolved.

- [ ] **Step 3: Update the NeedsCommissioning-mode LED bullet**

The remaining "Known scaffolding debt" bullet about `BootMode::NeedsCommissioning` having no visual signal (currently references "backlog #25" for the status-LED GPIO decision) is **not** resolved by this plan — this plan wires the identify/collision LED for `BootMode::Normal` only, not a `NeedsCommissioning`-mode indicator. Update that bullet to say the GPIO pin question is now resolved (GPIO 2, same physical LED used by field identification) but a distinct `NeedsCommissioning` blink pattern is still unimplemented and undeferred to any numbered backlog item — replace the words "deferred to backlog #25, which needs a status-LED GPIO pin decision first" with "not yet implemented — the status LED (GPIO 2, wired for field identification/collision-error blinking) could reuse the same physical LED with a third pattern, but no task currently owns this."

- [ ] **Step 4: Commit**

```bash
git add docs/task-status.md
git commit -m "$(cat <<'EOF'
. d Mark field identification wiring complete in task-status.md

EOF
)"
```
