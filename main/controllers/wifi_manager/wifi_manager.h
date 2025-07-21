/**
 * @file wifi_manager.h
 * @brief Manages the WiFi connection in Station (STA) mode and SNTP time sync.
 *
 * Handles initialization, connection with auto-reconnect, and provides an RTOS
 * event group for other tasks to synchronize with network and time readiness.
 */
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Event group bit: set when WiFi is connected and has an IP address. */
extern const int WIFI_CONNECTED_BIT;
/** @brief Event group bit: set when system time is synchronized via SNTP. */
extern const int TIME_SYNC_BIT;

/**
 * @brief Initializes the WiFi manager and starts connecting in Station (STA) mode.
 * Uses credentials from `secret.h`. Requires NVS and the default event loop
 * to be initialized first.
 */
void wifi_manager_init_sta(void);

/**
 * @brief Deinitializes the WiFi manager, stopping SNTP and disconnecting.
 */
void wifi_manager_deinit_sta(void);

/**
 * @brief Checks for full network readiness (connected to WiFi and time synced).
 * @return true if both connected and time-synced, false otherwise.
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Gets the current IP address of the device as a string.
 * @param buffer A character buffer to store the IP address (at least 16 bytes).
 * @param buffer_size The size of the provided buffer.
 * @return true if IP was retrieved, false if not connected or buffer is invalid.
 */
bool wifi_manager_get_ip_address(char* buffer, size_t buffer_size);

/**
 * @brief Gets the handle of the WiFi event group.
 * Allows other tasks to wait for network events using `xEventGroupWaitBits`.
 * @return The EventGroupHandle_t, or NULL if not initialized.
 */
EventGroupHandle_t wifi_manager_get_event_group(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H