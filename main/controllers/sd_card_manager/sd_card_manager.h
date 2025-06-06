#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <stdbool.h>

/**
 * @brief Inicializa el dispositivo de la tarjeta SD en el bus SPI ya configurado.
 *
 * Esta función debe ser llamada DESPUÉS de que el bus SPI (LCD_HOST) haya sido
 * inicializado (por ejemplo, por el screen_manager).
 *
 * @return true si la tarjeta se montó correctamente, false en caso de error.
 */
bool sd_manager_init(void);

/**
 * @brief Desmonta el sistema de archivos de la tarjeta SD y libera los recursos.
 */
void sd_manager_deinit(void);

/**
 * @brief Comprueba si la tarjeta SD está actualmente montada y lista para usarse.
 *
 * @return true si la tarjeta está montada, false en caso contrario.
 */
bool sd_manager_is_mounted(void);

/**
 * @brief Obtiene el punto de montaje de la tarjeta SD.
 *
 * @return Una cadena de caracteres con la ruta del punto de montaje (p.ej., "/sdcard").
 */
const char* sd_manager_get_mount_point(void);

/**
 * @brief Lista los archivos y directorios de una ruta específica en la tarjeta SD.
 *
 * Los resultados se imprimen en la consola de LOG.
 *
 * @param path La ruta del directorio a listar.
 */
void sd_manager_list_files(const char *path);

#endif // SD_CARD_MANAGER_H