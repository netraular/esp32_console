#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <stdbool.h>

/**
 * @brief Prototipo de la función callback para iterar sobre los archivos.
 * 
 * @param name El nombre del archivo o directorio.
 * @param is_dir true si la entrada es un directorio, false si es un archivo.
 * @param user_data Puntero a datos de usuario para pasar contexto (p.ej., el widget padre de LVGL).
 */
typedef void (*file_iterator_cb_t)(const char *name, bool is_dir, void *user_data);

/**
 * @brief Inicializa el dispositivo de la tarjeta SD en el bus SPI ya configurado.
 * @return true si la tarjeta se montó correctamente, false en caso de error.
 */
bool sd_manager_init(void);

/**
 * @brief Desmonta el sistema de archivos de la tarjeta SD y libera los recursos.
 */
void sd_manager_deinit(void);

/**
 * @brief Comprueba si la tarjeta SD está actualmente montada y lista para usarse.
 * @return true si la tarjeta está montada, false en caso contrario.
 */
bool sd_manager_is_mounted(void);

/**
 * @brief Obtiene el punto de montaje de la tarjeta SD.
 * @return Una cadena de caracteres con la ruta del punto de montaje (p.ej., "/sdcard").
 */
const char* sd_manager_get_mount_point(void);

/**
 * @brief Itera sobre los archivos y directorios de una ruta y llama a un callback por cada uno.
 *
 * @param path La ruta del directorio a listar.
 * @param callback La función a llamar por cada entrada encontrada.
 * @param user_data Un puntero de datos que se pasará al callback.
 */
void sd_manager_list_files(const char *path, file_iterator_cb_t callback, void *user_data);

#endif // SD_CARD_MANAGER_H