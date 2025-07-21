/**
 * @file file_explorer.h
 * @brief A reusable LVGL component that provides a file and directory browser.
 *
 * This component interacts with a parent view through callbacks for file selection,
 * navigation, and other actions. It manages its own input handling and state.
 */
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
    ITEM_TYPE_FILE,                 //!< A regular file.
    ITEM_TYPE_DIR,                  //!< A directory.
    ITEM_TYPE_PARENT_DIR,           //!< The ".." entry to navigate up.
    ITEM_TYPE_ACTION_CREATE_FILE,   //!< A user action to create a file.
    ITEM_TYPE_ACTION_CREATE_FOLDER, //!< A user action to create a folder.
} file_item_type_t;

/** @brief Callback for when a file is selected (OK press). */
typedef void (*file_select_callback_t)(const char *file_path, void* user_data);

/** @brief Callback for when a file is long-pressed. */
typedef void (*file_long_press_callback_t)(const char *file_path, void* user_data);

/** @brief Callback for when an action item is selected. */
typedef void (*file_action_callback_t)(file_item_type_t action_type, const char *current_path, void* user_data);

/** @brief Callback for when the user exits the explorer (navigates up from root). */
typedef void (*file_explorer_exit_callback_t)(void* user_data);

/**
 * @brief Creates the file explorer UI and registers its input handlers.
 *
 * @param parent The parent LVGL object to create the explorer on.
 * @param initial_path The starting directory path to display.
 * @param on_select Callback for when a file is selected via OK press.
 * @param on_long_press Callback for when a file is long-pressed. Can be NULL.
 * @param on_action Callback for when an action item is selected. Can be NULL.
 * @param on_exit Callback for when the user navigates back from the root directory.
 * @param user_data A user-defined pointer passed to all callbacks, for context.
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
 * Call this when the parent view is being destroyed.
 */
void file_explorer_destroy(void);

/**
 * @brief Forces a refresh of the file list for the current directory.
 * Useful after a file operation like create, delete, or rename.
 */
void file_explorer_refresh(void);

/**
 * @brief Activates or deactivates the file explorer's button input handlers.
 *
 * Use this to temporarily disable the explorer's navigation when, for example,
 * a popup dialog is shown over it.
 *
 * @param active true to activate input, false to deactivate.
 */
void file_explorer_set_input_active(bool active);

#ifdef __cplusplus
}
#endif

#endif // FILE_EXPLORER_H