/**
 * @file power_manager.h
 * @brief Manages the device's power states (Light Sleep and Deep Sleep).
 *
 * Provides simple functions to enter low-power modes, essential for
 * battery-powered operation.
 */
#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enters Light Sleep mode, configuring the ON/OFF button as a wake-up source.
 *
 * The CPU is paused, but RAM and peripheral states are retained. Execution
 * resumes from the point of the call after the wake-up button is pressed.
 * Button inputs are temporarily paused to prevent spurious wake-up events.
 */
void power_manager_enter_light_sleep(void);

/**
 * @brief Enters Deep Sleep mode indefinitely for a "full shutdown".
 *
 * The device enters its lowest power state. In this configuration, it can only
 * be awakened by an external reset (e.g., RST button).
 * This function does not return.
 */
void power_manager_enter_deep_sleep(void);


#ifdef __cplusplus
}
#endif

#endif // POWER_MANAGER_H