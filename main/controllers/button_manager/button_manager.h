#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "iot_button.h"
#include "button_gpio.h"
#include "config.h"

// Enum to uniquely identify each button
typedef enum {
    BUTTON_LEFT = 0,
    BUTTON_CANCEL,    
    BUTTON_OK,
    BUTTON_RIGHT,
    BUTTON_ON_OFF,
    BUTTON_COUNT // Total number of buttons, useful for loops
} button_id_t;

// Function type for button event handlers (callbacks)
typedef void (*button_handler_t)(void);

// Structure to manage default and view-specific handlers
typedef struct {
    button_handler_t default_handler;
    button_handler_t view_handler;
} button_handlers_t;

/**
 * @brief Initializes the button manager, configures GPIO pins, and registers default handlers.
 */
void button_manager_init();

/**
 * @brief Registers a default handler for a specific button.
 * This handler is used when no specific view handler is active.
 * @param button The ID of the button.
 * @param handler The callback function to execute.
 */
void button_manager_register_default_handler(button_id_t button, button_handler_t handler);

/**
 * @brief Registers a handler specific to a "view" or screen.
 * This handler takes priority over the default handler.
 * @param button The ID of the button.
 * @param handler The callback function to execute.
 */
void button_manager_register_view_handler(button_id_t button, button_handler_t handler);

/**
 * @brief Unregisters all view handlers and restores the default handlers.
 * Useful when changing screens in the UI.
 */
void button_manager_unregister_view_handlers();

#endif // BUTTON_MANAGER_H