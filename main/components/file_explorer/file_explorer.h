#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enum to identify the type of an item in the file explorer list.
 */
typedef enum {
    ITEM_TYPE_FILE,
    ITEM_TYPE_DIR,
    ITEM_TYPE_PARENT_DIR,
    ITEM_TYPE_ACTION_CREATE_FILE,
    ITEM_TYPE_ACTION_CREATE_FOLDER,
} file_item_type_t;

// --- MODIFIED: ALL CALLBACKS NOW ACCEPT user_data ---

/**
 * @brief Callback function type for when a file is selected (OK press).
 * @param file_path The full path of the selected file.
 * @param user_data A user-provided pointer passed during creation.
 */
typedef void (*file_select_callback_t)(const char *file_path, void* user_data);

/**
 * @brief Callback function type for when a file is long-pressed (OK long press).
 * @param file_path The full path of the long-pressed file.
 * @param user_data A user-provided pointer passed during creation.
 */
typedef void (*file_long_press_callback_t)(const char *file_path, void* user_data);

/**
 * @brief Callback function type for when an action item is selected.
 * @param action_type The type of action selected (e.g., ITEM_TYPE_ACTION_CREATE_FILE).
 * @param current_path The path of the directory where the action should be performed.
 * @param user_data A user-provided pointer passed during creation.
 */
typedef void (*file_action_callback_t)(file_item_type_t action_type, const char *current_path, void* user_data);

/**
 * @brief Callback function type for when the user exits the explorer.
 * @param user_data A user-provided pointer passed during creation.
 */
typedef void (*file_explorer_exit_callback_t)(void* user_data);

/**
 * @brief Creates the file explorer component UI and registers its input handlers.
 *
 * @param parent The parent LVGL object on which to create the explorer.
 * @param initial_path The starting directory path to display.
 * @param on_select Callback executed when a file is selected (TAP on OK).
 * @param on_long_press Callback executed when a file is long-pressed (LONG_PRESS on OK). Can be NULL.
 * @param on_action Callback executed when an action item is selected. Can be NULL.
 * @param on_exit Callback executed when the user navigates back from the root directory.
 * @param user_data A user-defined pointer that will be passed to all callback functions.
 */
void file_explorer_create(
    lv_obj_t *parent,
    const char *initial_path,
    file_select_callback_t on_select,
    file_long_press_callback_t on_long_press,
    file_action_callback_t on_action,
    file_explorer_exit_callback_t on_exit,
    void* user_data
);

/**
 * @brief Destroys the file explorer component and frees all associated resources.
 */
void file_explorer_destroy(void);

/**
 * @brief Forces a refresh of the file list for the current directory.
 */
void file_explorer_refresh(void);

/**
 * @brief Activates or deactivates the file explorer's button input handlers.
 * @param active true to activate input, false to deactivate.
 */
void file_explorer_set_input_active(bool active);

#ifdef __cplusplus
}
#endif

#endif // FILE_EXPLORER_H