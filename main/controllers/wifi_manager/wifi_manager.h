#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the WiFi manager and starts the connection process in STA mode.
 * Requires NVS to be initialized first.
 */
void wifi_manager_init_sta(void);

/**
 * @brief Disconnects from WiFi, stops the STA, and releases all related resources.
 */
void wifi_manager_deinit_sta(void);

/**
 * @brief Checks if the WiFi is connected and an IP address has been obtained.
 * @return true if connected, false otherwise.
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Gets the current IP address as a string.
 * @param buffer The buffer to store the IP address string.
 * @param buffer_size The size of the provided buffer.
 * @return true if the IP address was successfully retrieved, false otherwise.
 */
bool wifi_manager_get_ip_address(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H