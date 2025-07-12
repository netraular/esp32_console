#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- CORRECCIÓN: Exponer los bits del event group ---
extern const int WIFI_CONNECTED_BIT;
extern const int TIME_SYNC_BIT;

void wifi_manager_init_sta(void);
void wifi_manager_deinit_sta(void);
bool wifi_manager_is_connected(void);
bool wifi_manager_get_ip_address(char* buffer, size_t buffer_size);

/**
 * @brief Obtiene el manejador del event group de WiFi.
 * Permite a otras tareas esperar a los bits WIFI_CONNECTED_BIT y TIME_SYNC_BIT.
 * @return El EventGroupHandle_t o NULL si no está inicializado.
 */
EventGroupHandle_t wifi_manager_get_event_group(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H