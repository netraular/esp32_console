#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configures the ON/OFF button as a wake-up source and enters Light Sleep mode.
 * 
 * This function performs the following steps:
 * 1. Configures the ON/OFF button pin to wake the ESP32 on a low level.
 * 2. Calls `esp_light_sleep_start()` to put the device into a low-power state.
 * 3. Upon wake-up, logs the cause and disables the GPIO wake-up configuration.
 */
void power_manager_enter_light_sleep(void);

/**
 * @brief Enters Deep Sleep mode indefinitely.
 * 
 * The device will enter its lowest power mode and can only be awakened
 * by an external reset (RST button) or if deep sleep wake sources are
 * configured (not implemented in this function for a "permanent" shutdown).
 * This function does not return.
 */
void power_manager_enter_deep_sleep(void);


#ifdef __cplusplus
}
#endif

#endif // POWER_MANAGER_H