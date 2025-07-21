# Power Manager

## Description
This component manages the device's power states, specifically light sleep and deep sleep. It provides simple functions to reduce power consumption, making it essential for battery-powered operation.

## Features
-   **Light Sleep:** Provides a function (`power_manager_enter_light_sleep`) that puts the device into a low-power state where the CPU is paused, but RAM and peripherals are retained.
    -   It configures a GPIO pin (the ON/OFF button) as the wake-up source.
    -   It automatically pauses button input before sleeping and for a short period after waking to prevent spurious events.
-   **Deep Sleep:** Provides a function (`power_manager_enter_deep_sleep`) to put the device into its lowest power state. In this implementation, it's used for a "full shutdown" as the only way to wake up is via an external reset.

## How to Use

1.  **Enter Light Sleep (Standby):**
    Call this function to put the device to sleep. The code execution will pause and resume from the same point after the wake-up button is pressed.
    ```cpp
    #include "controllers/power_manager/power_manager.h"
    
    // Typically called from a view when the user presses the power button
    power_manager_enter_light_sleep();
    
    // Code resumes here after waking up
    ESP_LOGI(TAG, "Woke up!");
    ```

2.  **Enter Deep Sleep (Shutdown):**
    Call this to turn the device off. **This function does not return.** The device will need to be powered off and on, or reset using the RST button.
    ```cpp
    // Called when the user confirms they want to shut down the device
    power_manager_enter_deep_sleep();
    
    // This part of the code will not be reached
    ```

## Dependencies
-   `esp_sleep` component from ESP-IDF.
-   `button_manager` to pause input during the wake-up transition.
-   Requires the wake-up pin (`BUTTON_ON_OFF_PIN`) to be defined in `config.h`.