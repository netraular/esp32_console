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
    ITEM_TYPE_FILE,                 // A regular file.
    ITEM_TYPE_DIR,                  // A directory.
    ITEM_TYPE_PARENT_DIR,           // The ".." entry to go up one level.
    ITEM_TYPE_ACTION_CREATE_FILE,   // An action item to create a file.
    ITEM_TYPE_ACTION_CREATE_FOLDER, // An action item to create a folder.
} file_item_type_t;

/**
 * @brief Callback function type for when a file is selected (OK press).
 * @param file_path The full path of the selected file.
 */
typedef void (*file_select_callback_t)(const char *file_path);

/**
 * @brief Callback function type for when a file is long-pressed (OK long press).
 * @param file_path The full path of the long-pressed file.
 */
typedef void (*file_long_press_callback_t)(const char *file_path);

/**
 * @brief Callback function type for when an action item is selected.
 * @param action_type The type of action selected (e.g., ITEM_TYPE_ACTION_CREATE_FILE).
 * @param current_path The path of the directory where the action should be performed.
 */
typedef void (*file_action_callback_t)(file_item_type_t action_type, const char *current_path);

/**
 * @brief Callback function type for when the user exits the explorer (backs out of the root directory).
 */
typedef void (*file_explorer_exit_callback_t)(void);

/**
 * @brief Creates the file explorer component UI and registers its input handlers.
 *
 * @param parent The parent LVGL object on which to create the explorer.
 * @param initial_path The starting directory path to display.
 * @param on_select Callback executed when a file is selected (TAP on OK).
 * @param on_long_press Callback executed when a file is long-pressed (LONG_PRESS on OK). Can be NULL.
 * @param on_action Callback executed when an action item (e.g., "Create File") is selected. Can be NULL.
 * @param on_exit Callback executed when the user navigates back from the root directory.
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
 * @brief Destroys the file explorer component and frees all associated resources.
 */
void file_explorer_destroy(void);

/**
 * @brief Forces a refresh of the file list for the current directory.
 * Useful after an external file operation (create, delete, rename) has occurred.
 */
void file_explorer_refresh(void);

/**
 * @brief Activates or deactivates the file explorer's button input handlers.
 * Use this to give input control to the explorer or take it away for another component (like a popup).
 * @param active true to activate input, false to deactivate.
 */
void file_explorer_set_input_active(bool active);

#ifdef __cplusplus
}
#endif

#endif // FILE_EXPLORER_H