#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include "lvgl.h"

// --- Callback Types ---

/**
 * @brief Callback executed when the user selects a file.
 * @param file_path The full path of the selected file.
 */
typedef void (*file_select_callback_t)(const char *file_path);

/**
 * @brief Callback executed when the user exits the explorer 
 * (e.g., by pressing "Cancel" in the root directory).
 */
typedef void (*file_explorer_exit_callback_t)(void);


// --- Public Component Functions ---

/**
 * @brief Creates the file explorer interface.
 * 
 * @param parent The parent LVGL object where the explorer will be created.
 * @param initial_path The initial path to start browsing from (e.g., "/sdcard").
 * @param on_select Callback for when a file is selected.
 * @param on_exit Callback for when the user exits the explorer.
 */
void file_explorer_create(
    lv_obj_t *parent, 
    const char *initial_path, 
    file_select_callback_t on_select, 
    file_explorer_exit_callback_t on_exit
);

/**
 * @brief Destroys the file explorer and releases all its resources.
 * It's crucial to call this function to prevent memory leaks and dangling pointers.
 */
void file_explorer_destroy(void);

#endif // FILE_EXPLORER_H