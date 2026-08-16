#include <Arduino.h>

#include "adapters/ControllerNode.h"

// ControllerNode is constructed here (function-local static, not file-scope)
// because its constructor performs real NVS/GPIO work via NvsConfigStore and
// the Esp*Output/Input adapters. A file-scope static would run before
// app_main() calls initArduino() (see cores/esp32/main.cpp), which is the
// only place nvs_flash_init() runs (esp32-hal-misc.c) - constructing before
// that silently drops all saved NVS config to factory defaults. setup() only
// runs after initArduino(), so this ordering is safe.
static ControllerNode* node = nullptr;

void setup()
{
    static ControllerNode instance;
    node = &instance;
    node->begin();
}

void loop()
{
    node->tick();
}
