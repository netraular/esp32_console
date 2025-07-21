/**
 * @file button_manager.h
 * @brief Manages physical button inputs with advanced event handling.
 *
 * This controller abstracts the `espressif/button` library to provide a rich
 * set of events (tap, single/double click, long press) and a two-tier
 * (default vs. view-specific) callback system. Events are queued and dispatched
 * in the LVGL context for UI safety.
 */
#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "iot_button.h"
#include "button_gpio.h"
#include "lvgl.h"

/**
 * @brief Enum to uniquely identify each physical button.
 */
typedef enum {
    BUTTON_LEFT = 0,    //!< Left button.
    BUTTON_CANCEL,      //!< Cancel/Back button.
    BUTTON_OK,          //!< OK/Select button.
    BUTTON_RIGHT,       //!< Right button.
    BUTTON_ON_OFF,      //!< Power/Special function button.
    BUTTON_COUNT        //!< Total number of buttons.
} button_id_t;

/**
 * @brief Enum for the dispatch mode of button events.
 */
typedef enum {
    INPUT_DISPATCH_MODE_QUEUED,   //!< (Default) Events are queued and processed via an LVGL timer. UI-safe.
    INPUT_DISPATCH_MODE_IMMEDIATE //!< Events execute callbacks instantly. For low-latency needs.
} input_dispatch_mode_t;

/**
 * @brief Enum for the specific, abstracted event types handled by the manager.
 */
typedef enum {
    BUTTON_EVENT_PRESS_DOWN,       //!< Button is pressed down.
    BUTTON_EVENT_PRESS_UP,         //!< Button is released.
    BUTTON_EVENT_SINGLE_CLICK,     //!< A single click (fired after double-click timeout).
    BUTTON_EVENT_DOUBLE_CLICK,     //!< Two quick clicks.
    BUTTON_EVENT_LONG_PRESS_START, //!< Button held for the long-press duration.
    BUTTON_EVENT_LONG_PRESS_HOLD,  //!< Fired repeatedly while held after a long-press start.
    BUTTON_EVENT_TAP,              //!< Fires immediately on press-up (if not a long press).
    BUTTON_EVENT_COUNT             //!< Total number of event types.
} button_event_type_t;

/**
 * @brief Function pointer for button event handlers.
 * @param user_data A user-provided pointer passed during handler registration.
 */
typedef void (*button_handler_t)(void* user_data);


/**
 * @brief Initializes the button manager. Must be called once at startup.
 */
void button_manager_init();

/**
 * @brief Sets the dispatch mode for button events.
 * @param mode The mode to use (QUEUED or IMMEDIATE).
 */
void button_manager_set_dispatch_mode(input_dispatch_mode_t mode);

/**
 * @brief Registers a handler for a specific button and event type.
 *
 * @param button The ID of the button.
 * @param event The type of event to handle.
 * @param handler The callback function to execute.
 * @param is_view_handler If true, registers as a high-priority handler that is cleared
 *                        by `unregister_view_handlers`. If false, registers as a
 *                        low-priority default handler.
 * @param user_data A pointer passed to the handler, for context (e.g., `this`).
 */
void button_manager_register_handler(button_id_t button, button_event_type_t event, button_handler_t handler, bool is_view_handler, void* user_data);

/**
 * @brief Unregisters all view-specific handlers and clears the event queue.
 * Crucial to call when changing views to restore default button behaviors.
 */
void button_manager_unregister_view_handlers();

/**
 * @brief Temporarily pauses button event processing, ideal for after wake-up.
 *
 * Clears the event queue and ignores inputs for a specified duration to prevent
 * spurious events from the button press that woke the device.
 *
 * @param pause_ms The duration in milliseconds to ignore button inputs.
 */
void button_manager_pause_for_wake_up(uint32_t pause_ms);


#endif // BUTTON_MANAGER_H