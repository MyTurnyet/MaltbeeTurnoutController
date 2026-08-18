#include <Arduino.h>

#include "adapters/ControllerNode.h"
#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
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
// Short-press window for the runtime identify-blink trigger (field
// identification) - reuses the same physical BOOT pin, per
// docs/software-class-list.md's "Field Identification: Blink-Out" design.
constexpr unsigned long kIdentifyMinPressMs = 50;
constexpr unsigned long kIdentifyMaxPressMs = 1500;

// How long BOOT must be held, during normal runtime, before releasing it
// re-enters wireless setup on the next boot. Deliberately well above
// kIdentifyMaxPressMs above so a single release can never satisfy both
// triggers. Read live in loop() rather than at boot time - see
// ButtonSetupModeTrigger.h for why holding BOOT through an ESP32 power-on
// can't be detected in application code at all.
constexpr unsigned long kSetupModeHoldMs = 3000;

// How long the identify-blink sequence stays active after a qualifying
// short press, before the status LED goes dark again.
constexpr unsigned long kIdentifyActiveMs = 5000;

// Blink half-period for the distinct collision-error pattern - steady fast
// blink, visually different from the per-id identify pattern.
constexpr unsigned long kCollisionBlinkHalfPeriodMs = 250;

// Blink half-period for the setup-mode indicator - rapid steady blink,
// faster than the collision pattern above so the two are distinguishable
// even though they never occur at the same time (mutually exclusive boot
// modes). Lets a customer match the AP name in their WiFi list to the
// physical board - see docs/software-class-list.md's "Entering Setup Mode".
constexpr unsigned long kSetupModeBlinkHalfPeriodMs = 100;
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
static SteadyBlinker* setupModeBlinker = nullptr;
static ButtonSetupModeTrigger* runtimeSetupTrigger = nullptr;
static NvsSetupModeRequestStore* setupRequestStore = nullptr;
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

    static NvsSetupModeRequestStore requestStore;
    setupRequestStore = &requestStore;
    bool wirelessSetupRequested = requestStore.consumeRequest();

    NodeConfig config = commissioningStore.load();
    BootMode mode = BootModeSelector::select(config, wirelessSetupRequested);

    // Shared with the runtime identify/setup-mode triggers below - same
    // physical BOOT pin, distinguished by hold duration.
    static EspDigitalInput bootPin(0, true);

    // Status LED (GPIO 2) and its clock are shared across all three boot
    // modes - only the pattern driving them differs.
    static EspDigitalOutput led(2);
    statusLed = &led;
    static ArduinoClock ledClock;
    blinkClock = &ledClock;

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

        // Rapid steady blink signals "this board is in setup mode" - see
        // kSetupModeBlinkHalfPeriodMs above.
        static SteadyBlinker setupBlinker{Duration(kSetupModeBlinkHalfPeriodMs)};
        setupModeBlinker = &setupBlinker;
    }
    else
    {
        // Normal and NeedsCommissioning both watch BOOT for the runtime
        // gesture that (re-)enters wireless setup: hold for
        // kSetupModeHoldMs, then release. This is the only way a
        // factory-fresh board (NeedsCommissioning, no valid config yet)
        // reaches wireless setup at all.
        static ButtonSetupModeTrigger setupTrigger(bootPin, Duration(kSetupModeHoldMs));
        runtimeSetupTrigger = &setupTrigger;

        if (mode == BootMode::Normal)
        {
            static ControllerNode instance;
            node = &instance;
            node->begin();

            // Field identification (short-press BOOT blinks the node's id)
            // and the distinct collision-error pattern (steady fast blink,
            // driven instead whenever ControllerNode::blocked() is true)
            // share the status LED - only meaningful once a node has an
            // actual id, so these are only constructed in Normal mode.
            static ButtonIdentifyRequestTrigger trigger(bootPin, Duration(kIdentifyMinPressMs), Duration(kIdentifyMaxPressMs));
            identifyTrigger = &trigger;
            static BlinkOutIdentifier identifier(config.id(), Duration(200), Duration(200), Duration(1000));
            blinkIdentifier = &identifier;
            static SteadyBlinker errorBlinker{Duration(kCollisionBlinkHalfPeriodMs)};
            collisionBlinker = &errorBlinker;
        }
    }
}

void loop()
{
    Instant now = blinkClock->now();

    if (runtimeSetupTrigger != nullptr)
    {
        runtimeSetupTrigger->poll(now);
        if (runtimeSetupTrigger->requested())
        {
            // BOOT was already observed released (see ButtonSetupModeTrigger)
            // before this fires, so it's safe to restart here - GPIO0 won't
            // be held low at the ROM's next strapping-pin sample.
            setupRequestStore->requestOnNextBoot();
            ESP.restart();
        }
    }

    if (node != nullptr)
    {
        node->tick();

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
        statusLed->write(setupModeBlinker->levelAt(now - Instant(0)));

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
