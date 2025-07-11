#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Tipos de datos para identificar los elementos de la lista
typedef enum {
    ITEM_TYPE_FILE,
    ITEM_TYPE_DIR,
    ITEM_TYPE_PARENT_DIR,
    ITEM_TYPE_ACTION_CREATE_FILE,
    ITEM_TYPE_ACTION_CREATE_FOLDER,
} file_item_type_t;

// Callbacks para interactuar con la vista que lo utiliza
typedef void (*file_select_callback_t)(const char *file_path);
typedef void (*file_long_press_callback_t)(const char *file_path);
typedef void (*file_action_callback_t)(file_item_type_t action_type, const char *current_path);
typedef void (*file_explorer_exit_callback_t)(void);

/**
 * @brief Crea el componente explorador de archivos.
 *
 * @param parent El objeto padre LVGL sobre el que se creará el explorador.
 * @param initial_path La ruta inicial a mostrar.
 * @param on_select Callback que se ejecuta cuando se selecciona un archivo (no un directorio).
 * @param on_long_press Callback que se ejecuta cuando se mantiene presionado OK sobre un archivo. Puede ser NULL.
 * @param on_action Callback que se ejecuta cuando se selecciona una acción (ej. "Crear Archivo"). Puede ser NULL si no se necesita.
 * @param on_exit Callback que se ejecuta cuando el usuario navega hacia atrás desde la raíz.
 */
void file_explorer_create(
    lv_obj_t *parent,
    const char *initial_path,
    file_select_callback_t on_select,
    file_long_press_callback_t on_long_press,
    file_action_callback_t on_action,
    file_explorer_exit_callback_t on_exit
);

/**
 * @brief Destruye el explorador de archivos y libera sus recursos.
 */
void file_explorer_destroy(void);

/**
 * @brief Refresca el contenido de la lista del explorador.
 * Útil después de crear o eliminar archivos.
 */
void file_explorer_refresh(void);

/**
 * @brief Activa o desactiva los manejadores de entrada del explorador.
 *
 * @param active true para activar la entrada, false para desactivarla.
 */
void file_explorer_set_input_active(bool active);

#ifdef __cplusplus
}
#endif

#endif // FILE_EXPLORER_H