#include <Arduino.h>

#include "adapters/ControllerNode.h"
#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "domain/CommissioningSession.h"
#include "domain/NodeConfig.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"

// ControllerNode is constructed here (function-local static, not file-scope)
// because its constructor performs real NVS/GPIO work via NvsConfigStore and
// the Esp*Output/Input adapters. A file-scope static would run before
// app_main() calls initArduino() (see cores/esp32/main.cpp), which is the
// only place nvs_flash_init() runs (esp32-hal-misc.c) - constructing before
// that silently drops all saved NVS config to factory defaults. setup() only
// runs after initArduino(), so this ordering is safe.
static ControllerNode* node = nullptr;
static SerialCommissioningAdapter* commissioningAdapter = nullptr;

void setup()
{
    // Bench serial commissioning runs in parallel with normal operation
    // regardless of config validity, so a technician can always recover a
    // board stuck in NeedsCommissioning mode by plugging in USB - it's a
    // distinct physical channel (UART) from the turnout GPIO/MQTT graph.
    static NvsConfigStore commissioningStore;
    static CommissioningSession commissioningSession(commissioningStore);
    static EspUartPort uart(115200);
    static SerialCommissioningAdapter adapter(uart, commissioningSession);
    commissioningAdapter = &adapter;

    // Constructing ControllerNode wires real GPIO pins from NodeConfig - on a
    // factory-default or otherwise invalid config (e.g. NodeId(0), sentinel
    // pin -1 on every turnout), that would build EspDigitalOutput/
    // EspDigitalInput against pin -1, which is untested and unsafe. Check
    // validity first and skip building the hardware graph entirely if it
    // would be unsafe - node stays nullptr, loop() only runs the always-on
    // serial commissioning channel until a technician saves a valid config
    // and reboots.
    NodeConfig config = commissioningStore.load();
    if (BootModeSelector::select(config) == BootMode::Normal)
    {
        static ControllerNode instance;
        node = &instance;
        node->begin();
    }
}

void loop()
{
    if (node != nullptr)
    {
        node->tick();
    }

    commissioningAdapter->poll();
    if (commissioningAdapter->rebootRequested())
    {
        ESP.restart();
    }
}
