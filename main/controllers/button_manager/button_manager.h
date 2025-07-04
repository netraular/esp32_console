#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "iot_button.h"
#include "button_gpio.h"
#include "config.h"
#include "lvgl.h"

// Enum to uniquely identify each button
typedef enum {
    BUTTON_LEFT = 0,
    BUTTON_CANCEL,
    BUTTON_OK,
    BUTTON_RIGHT,
    BUTTON_ON_OFF,
    BUTTON_COUNT // Total number of buttons, useful for loops
} button_id_t;

// Enum for the dispatch mode of button events
typedef enum {
    INPUT_DISPATCH_MODE_QUEUED,     // (Default) Events are queued and processed via an LVGL timer. UI-safe.
    INPUT_DISPATCH_MODE_IMMEDIATE   // Events execute the callback instantly. For low-latency needs (e.g., games).
} input_dispatch_mode_t;

// Enum for the specific event types we want to handle
typedef enum {
    BUTTON_EVENT_PRESS_DOWN,
    BUTTON_EVENT_PRESS_UP,
    BUTTON_EVENT_SINGLE_CLICK,
    BUTTON_EVENT_DOUBLE_CLICK,
    BUTTON_EVENT_LONG_PRESS_START,
    BUTTON_EVENT_LONG_PRESS_HOLD,
    BUTTON_EVENT_TAP, // A fast click event that fires on press-up.
    BUTTON_EVENT_COUNT // Total number of event types
} button_event_type_t;

// Function type for button event handlers (callbacks)
// MODIFIED: Accepts a user_data pointer
typedef void (*button_handler_t)(void* user_data);

// MODIFIED: Structure to store the handler and its user_data
typedef struct {
    button_handler_t handler;
    void* user_data;
} handler_entry_t;

// Structure to hold handlers for all possible event types
typedef struct {
    // MODIFIED: Now an array of handler_entry_t
    handler_entry_t handlers[BUTTON_EVENT_COUNT];
} button_event_handlers_t;

// Main structure to manage default and view-specific handlers for a single button
typedef struct {
    button_event_handlers_t default_handlers;
    button_event_handlers_t view_handlers;
} button_handlers_t;


/**
 * @brief Initializes the button manager, configures GPIO pins, and registers internal callbacks.
 * By default, operates in QUEUED mode.
 */
void button_manager_init();

/**
 * @brief Sets the dispatch mode for button events.
 *
 * @param mode The mode to use (QUEUED or IMMEDIATE).
 */
void button_manager_set_dispatch_mode(input_dispatch_mode_t mode);

/**
 * @brief Registers a handler for a specific button and event type.
 *
 * @param button The ID of the button.
 * @param event The type of event to handle.
 * @param handler The callback function to execute.
 * @param is_view_handler If true, this is a view-specific handler that takes priority. If false, it's a default handler.
 * @param user_data An optional pointer that will be passed to the handler function when it is executed.
 */
void button_manager_register_handler(button_id_t button, button_event_type_t event, button_handler_t handler, bool is_view_handler, void* user_data);

/**
 * @brief Unregisters all view-specific handlers and clears any pending button events from the queue.
 * Useful when changing screens in the UI to prevent stale events.
 */
void button_manager_unregister_view_handlers();


#endif // BUTTON_MANAGER_H