#include <Arduino.h>

#include "adapters/ControllerNode.h"

static ControllerNode node;

void setup()
{
    node.begin();
}

void loop()
{
    node.tick();
}
