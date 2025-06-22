#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include "lvgl.h"

// Tipos de datos y Callbacks (sin cambios)
typedef enum {
    ITEM_TYPE_FILE,
    ITEM_TYPE_DIR,
    ITEM_TYPE_PARENT_DIR,
    ITEM_TYPE_ACTION_CREATE_FILE,
    ITEM_TYPE_ACTION_CREATE_FOLDER,
} file_item_type_t;

typedef void (*file_select_callback_t)(const char *file_path);
typedef void (*file_action_callback_t)(file_item_type_t action_type, const char *current_path);
typedef void (*file_explorer_exit_callback_t)(void);

// Funciones Públicas (sin cambios)
void file_explorer_create(
    lv_obj_t *parent, 
    const char *initial_path, 
    file_select_callback_t on_select,
    file_action_callback_t on_action, 
    file_explorer_exit_callback_t on_exit
);

void file_explorer_destroy(void);
void file_explorer_refresh(void);
void file_explorer_set_input_active(bool active);

#endif // FILE_EXPLORER_H