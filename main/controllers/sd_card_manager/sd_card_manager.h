#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ... (funciones existentes sin cambios) ...
bool sd_manager_init(void);
bool sd_manager_mount(void);
void sd_manager_unmount(void);
void sd_manager_deinit(void);
bool sd_manager_is_mounted(void);

/**
 * @brief Comprueba si la tarjeta SD está lista para ser usada.
 * Intenta montar la tarjeta si no lo está y verifica que sea legible.
 * Si la tarjeta está montada pero no responde, la desmontará para permitir un reintento limpio.
 * @return true si la tarjeta está montada y es legible, false en caso contrario.
 */
bool sd_manager_check_ready(void);

const char* sd_manager_get_mount_point(void);

typedef void (*file_iterator_cb_t)(const char* name, bool is_dir, void* user_data);

bool sd_manager_list_files(const char* path, file_iterator_cb_t cb, void* user_data);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_MANAGER_H