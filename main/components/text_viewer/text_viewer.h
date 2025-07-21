/**
 * @file text_viewer.h
 * @brief A simple, full-screen component for displaying read-only text content.
 *
 * Ideal for showing text files, transcription results, or help information.
 * It automatically manages the memory of the text content provided to it.
 */
#ifndef TEXT_VIEWER_H
#define TEXT_VIEWER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function to notify when the user wants to exit the viewer.
 * @param user_data A user-provided pointer, passed during component creation.
 */
typedef void (*text_viewer_exit_callback_t)(void* user_data);

/**
 * @brief Creates a full-screen text viewer component.
 *
 * @warning This component takes ownership of the `content` pointer and will
 * automatically call `free()` on it when the viewer is destroyed.
 * Do NOT free the content buffer yourself after passing it to this function.
 *
 * @param parent The parent LVGL object (typically the active screen).
 * @param title The title to display at the top of the viewer.
 * @param content The text content to display. Must be a `NULL`-terminated string
 *                allocated with `malloc`, `calloc`, `strdup`, etc.
 * @param on_exit Callback function executed when the user presses the 'Cancel' button.
 * @param exit_cb_user_data A user-defined pointer passed to the `on_exit` callback for context.
 * @return A pointer to the main container object of the viewer.
 */
lv_obj_t* text_viewer_create(lv_obj_t* parent, const char* title, char* content, text_viewer_exit_callback_t on_exit, void* exit_cb_user_data);

/**
 * @brief Destroys the text viewer component and frees its resources.
 *
 * This calls `lv_obj_del()` on the viewer, which triggers an internal event
 * that frees the text content buffer and other allocated memory.
 *
 * @param viewer The main container object returned by `text_viewer_create`.
 */
void text_viewer_destroy(lv_obj_t* viewer);

#ifdef __cplusplus
}
#endif

#endif // TEXT_VIEWER_H