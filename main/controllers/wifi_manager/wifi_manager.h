#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Event group bit for successful WiFi connection and IP acquisition. */
extern const int WIFI_CONNECTED_BIT;
/** @brief Event group bit for successful time synchronization via SNTP. */
extern const int TIME_SYNC_BIT;

/**
 * @brief Initializes the WiFi manager and starts the connection process in Station (STA) mode.
 * It will automatically attempt to connect to the WiFi network specified in secret.h.
 */
void wifi_manager_init_sta(void);

/**
 * @brief Deinitializes the WiFi manager, stops SNTP, and disconnects from the network.
 */
void wifi_manager_deinit_sta(void);

/**
 * @brief Checks if the device is connected to WiFi and has synchronized its time.
 * This is the recommended check for full network readiness.
 * @return true if both connected and time-synced, false otherwise.
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Gets the current IP address of the device as a string.
 * @param buffer A character buffer to store the IP address string.
 * @param buffer_size The size of the provided buffer (should be at least 16).
 * @return true if the IP address was successfully retrieved, false if not connected or buffer is invalid.
 */
bool wifi_manager_get_ip_address(char* buffer, size_t buffer_size);

/**
 * @brief Gets the handle of the WiFi event group.
 * This allows other tasks to wait for specific network events using `xEventGroupWaitBits`.
 * @return The EventGroupHandle_t for the WiFi manager, or NULL if not initialized.
 */
EventGroupHandle_t wifi_manager_get_event_group(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H