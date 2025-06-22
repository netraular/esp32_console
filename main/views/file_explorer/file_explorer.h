#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include "lvgl.h"

// --- Tipos de datos y Callbacks ---

// Enum para identificar el tipo de cada elemento en la lista del explorador
typedef enum {
    ITEM_TYPE_FILE,
    ITEM_TYPE_DIR,
    ITEM_TYPE_PARENT_DIR, // Para el item ".."
    ITEM_TYPE_ACTION_CREATE_FILE,
    ITEM_TYPE_ACTION_CREATE_FOLDER,
} file_item_type_t;

// Callback para cuando se selecciona un archivo real
typedef void (*file_select_callback_t)(const char *file_path);
// Callback para cuando se selecciona una acción (ej. "Crear Archivo")
typedef void (*file_action_callback_t)(file_item_type_t action_type, const char *current_path);
// Callback para cuando el usuario sale del explorador
typedef void (*file_explorer_exit_callback_t)(void);

// --- Funciones Públicas del Componente ---

void file_explorer_create(
    lv_obj_t *parent, 
    const char *initial_path, 
    file_select_callback_t on_select,
    file_action_callback_t on_action, 
    file_explorer_exit_callback_t on_exit
);

void file_explorer_destroy(void);

/**
 * @brief Refresca la lista de archivos en el explorador.
 * Útil después de crear o eliminar un archivo.
 */
void file_explorer_refresh(void);

/**
 * @brief Activa o desactiva el control de entrada para el explorador.
 * Cuando se activa, registra sus propios manejadores de botones y establece su grupo como predeterminado.
 * Útil para que una vista modal pueda tomar el control temporalmente.
 * @param active true para activar, false para desactivar (liberando los botones para otro componente).
 */
void file_explorer_set_input_active(bool active);


#endif // FILE_EXPLORER_H