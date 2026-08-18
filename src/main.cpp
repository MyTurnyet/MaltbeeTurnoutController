#include <Arduino.h>

#include "adapters/ControllerNode.h"
#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ButtonSetupModeTrigger.h"
#include "adapters/EspDigitalInput.h"
#include "adapters/ArduinoClock.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "adapters/CaptivePortalServer.h"
#include "domain/CommissioningSession.h"
#include "domain/NodeConfig.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/Duration.h"
#include "domain/Instant.h"

namespace
{
// How long BOOT must be held through power-on to enter wireless setup mode.
// This is an unavoidable fixed delay on every boot, not just when BOOT is
// held - see this plan's Global Constraints for why.
constexpr unsigned long kBootWindowMs = 2000;

// GPIO 0 is the BOOT button on ESP32-WROOM-32 dev boards - active-low, tied
// high via internal pull-up when not pressed. Reading it here in setup() is
// well after the ROM bootloader's own strapping-pin decision has resolved.
bool detectWirelessSetupRequest()
{
    static ArduinoClock bootClock;
    static EspDigitalInput bootPin(0, true);
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
// runs after initArduino(), so this ordering is safe.
static ControllerNode* node = nullptr;
static SerialCommissioningAdapter* commissioningAdapter = nullptr;
static CaptivePortalServer* captivePortal = nullptr;
static WebFormCommissioningAdapter* webFormAdapter = nullptr;

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

    bool wirelessSetupRequested = detectWirelessSetupRequest();
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
    }

    // BootMode::NeedsCommissioning: neither node nor captivePortal is
    // constructed - loop() below only runs the always-on serial channel.
}

void loop()
{
    if (node != nullptr)
    {
        node->tick();
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
