#include <Arduino.h>

#include "adapters/ControllerNode.h"
#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "domain/CommissioningSession.h"

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
    static ControllerNode instance;
    node = &instance;
    node->begin();

    // Bench serial commissioning runs in parallel with normal operation, not
    // as an alternate boot mode - it's a distinct physical channel (UART)
    // from the turnout GPIO/MQTT graph ControllerNode owns, so a technician
    // can plug in and commission at any time, per docs/software-class-list.md's
    // "Plug into USB, open a serial terminal" workflow. Uses its own
    // NvsConfigStore instance (stateless per call, same "mcs-cfg" namespace
    // ControllerNode's own store reads) rather than reaching into
    // ControllerNode's internals.
    static NvsConfigStore commissioningStore;
    static CommissioningSession commissioningSession(commissioningStore);
    static EspUartPort uart(115200);
    static SerialCommissioningAdapter adapter(uart, commissioningSession);
    commissioningAdapter = &adapter;
}

void loop()
{
    node->tick();

    commissioningAdapter->poll();
    if (commissioningAdapter->rebootRequested())
    {
        ESP.restart();
    }
}
