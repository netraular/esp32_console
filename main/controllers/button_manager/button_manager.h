#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "iot_button.h"
#include "button_gpio.h"
#include "config.h"
#include "lvgl.h"

// ... (The rest of the enums and structs remain the same) ...

/**
 * @brief Enum to uniquely identify each physical button.
 */
typedef enum {
    BUTTON_LEFT = 0,
    BUTTON_CANCEL,
    BUTTON_OK,
    BUTTON_RIGHT,
    BUTTON_ON_OFF,
    BUTTON_COUNT // Total number of buttons, useful for loops
} button_id_t;

/**
 * @brief Enum for the dispatch mode of button events.
 */
typedef enum {
    INPUT_DISPATCH_MODE_QUEUED,     // (Default) Events are queued and processed via an LVGL timer. UI-safe.
    INPUT_DISPATCH_MODE_IMMEDIATE   // Events execute the callback instantly. For low-latency needs (e.g., games).
} input_dispatch_mode_t;

/**
 * @brief Enum for the specific, abstracted event types we handle.
 */
typedef enum {
    BUTTON_EVENT_PRESS_DOWN,       // Button is pressed down.
    BUTTON_EVENT_PRESS_UP,         // Button is released.
    BUTTON_EVENT_SINGLE_CLICK,     // A single click, fired after double-click timeout has passed.
    BUTTON_EVENT_DOUBLE_CLICK,     // Two quick clicks.
    BUTTON_EVENT_LONG_PRESS_START, // The button has been held down for the configured long-press duration.
    BUTTON_EVENT_LONG_PRESS_HOLD,  // Fired repeatedly while the button is held down after a long-press start.
    BUTTON_EVENT_TAP,              // A fast click event that fires immediately on press-up (if not a long press).
    BUTTON_EVENT_COUNT             // Total number of event types.
} button_event_type_t;

/**
 * @brief Function pointer type for button event handlers (callbacks).
 * @param user_data A user-provided pointer passed during handler registration.
 */
typedef void (*button_handler_t)(void* user_data);

/**
 * @brief Structure to store a handler and its associated user data.
 */
typedef struct {
    button_handler_t handler;
    void* user_data;
} handler_entry_t;

/**
 * @brief Structure to hold handlers for all possible event types for a button.
 */
typedef struct {
    handler_entry_t handlers[BUTTON_EVENT_COUNT];
} button_event_handlers_t;

/**
 * @brief Main structure to manage default and view-specific handlers for a single button.
 */
typedef struct {
    button_event_handlers_t default_handlers;
    button_event_handlers_t view_handlers;
} button_handlers_t;


/**
 * @brief Initializes the button manager, configures GPIO pins, and registers internal callbacks.
 * This must be called once at application startup. By default, it operates in QUEUED mode.
 */
void button_manager_init();

/**
 * @brief Sets the dispatch mode for button events.
 * @param mode The mode to use (QUEUED or IMMEDIATE). QUEUED is recommended for UI interactions.
 */
void button_manager_set_dispatch_mode(input_dispatch_mode_t mode);

/**
 * @brief Registers a handler for a specific button and event type.
 *
 * @param button The ID of the button.
 * @param event The type of event to handle.
 * @param handler The callback function to execute when the event occurs.
 * @param is_view_handler If true, registers a high-priority view-specific handler. If false, it's a low-priority default handler.
 * @param user_data An optional pointer that will be passed to the handler function when it is executed. Can be used to pass context (e.g., `this`).
 */
void button_manager_register_handler(button_id_t button, button_event_type_t event, button_handler_t handler, bool is_view_handler, void* user_data);

/**
 * @brief Unregisters all view-specific handlers and clears any pending button events from the queue.
 * This is crucial to call when changing views to prevent stale events and restore default button behaviors.
 */
void button_manager_unregister_view_handlers();

/**
 * @brief Temporarily pauses button event processing, ideal for after wake-up.
 * Clears the event queue and enters a paused state. An internal timer will
 * automatically resume processing after the specified duration.
 *
 * @param pause_ms The duration in milliseconds to ignore button inputs.
 */
void button_manager_pause_for_wake_up(uint32_t pause_ms);


#endif // BUTTON_MANAGER_H