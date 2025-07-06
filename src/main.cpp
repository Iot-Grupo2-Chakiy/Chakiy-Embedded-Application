#include "DeviceManager.h"

// Create device manager instance
DeviceManager device;


void setup() {
    device.setup();
}

void loop() {
    device.getCurrentTimeDevice();
    device.loop();
    }