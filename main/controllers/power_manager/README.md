# Power Manager

## Description
This component provides centralized control over the ESP32's power-saving modes, specifically Light Sleep and Deep Sleep.

## Features
- **Light Sleep:** A low-power mode where the CPU is paused, but RAM and peripherals are kept powered. The device can be woken up quickly by configured sources.
- **Deep Sleep:** The lowest power mode, where most of the chip is powered down. Wake-up is similar to a full reset.

## How to Use

### Entering Light Sleep
This is the primary mode for putting the device to "sleep" while preserving state. The device will wake up when the `ON/OFF` button is pressed.

```cpp
#include "controllers/power_manager/power_manager.h"
#include "controllers/button_manager/button_manager.h"

// Put the device to sleep
power_manager_enter_light_sleep();

// --- After waking up ---
// The device will resume execution from the line after the call above.
// It is CRITICAL to pause button input briefly to prevent the press that
// woke the device from triggering an immediate action.
ESP_LOGI(TAG, "Woke up from light sleep.");
button_manager_pause_for_wake_up(200); // Ignore button inputs for 200ms