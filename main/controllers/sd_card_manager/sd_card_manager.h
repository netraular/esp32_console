#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <stdbool.h>
#include <stddef.h> // Para size_t

#ifdef __cplusplus
extern "C" {
#endif

// --- Funciones existentes ---
bool sd_manager_init(void);
bool sd_manager_mount(void);
void sd_manager_unmount(void);
void sd_manager_deinit(void);
bool sd_manager_is_mounted(void);
bool sd_manager_check_ready(void);
const char* sd_manager_get_mount_point(void);

typedef void (*file_iterator_cb_t)(const char* name, bool is_dir, void* user_data);
bool sd_manager_list_files(const char* path, file_iterator_cb_t cb, void* user_data);

// --- Funciones de Gestión de Archivos ---

bool sd_manager_delete_item(const char* path);
bool sd_manager_rename_item(const char* old_path, const char* new_path);
bool sd_manager_create_directory(const char* path);
bool sd_manager_create_file(const char* path);
bool sd_manager_read_file(const char* path, char** buffer, size_t* size);

/**
 * @brief Escribe contenido de texto en un archivo, sobrescribiéndolo si existe.
 * @param path Ruta completa del archivo.
 * @param content El contenido de texto a escribir.
 * @return true en caso de éxito, false en caso de error.
 */
bool sd_manager_write_file(const char* path, const char* content);


#ifdef __cplusplus
}
#endif

#endif // SD_CARD_MANAGER_H