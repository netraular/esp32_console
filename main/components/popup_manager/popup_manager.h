/**
 * @file popup_manager.h
 * @brief A centralized manager for creating standardized, modal pop-up dialogs.
 *
 * This component provides a simple API to show common dialogs like alerts,
 * confirmations, and loading indicators. It automatically handles input focus,
 * ensuring that the underlying view's controls are disabled while a pop-up is active.
 */
#ifndef POPUP_MANAGER_H
#define POPUP_MANAGER_H

#include "lvgl.h"
#include <stdbool.h> // Include for bool type

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enum to indicate the result of a user interaction with a pop-up.
 */
typedef enum {
    POPUP_RESULT_PRIMARY,   //!< The primary action button was pressed (e.g., "OK", "Confirm").
    POPUP_RESULT_SECONDARY, //!< The secondary action button was pressed (e.g., "Cancel").
    POPUP_RESULT_DISMISSED  //!< The pop-up was dismissed without an action (e.g. physical cancel button).
} popup_result_t;

/**
 * @brief Callback function to handle the result of a pop-up interaction.
 *
 * @param result The action taken by the user.
 * @param user_data A user-provided pointer, passed during the pop-up creation call.
 */
typedef void (*popup_callback_t)(popup_result_t result, void* user_data);

/**
 * @brief Shows a simple alert pop-up with a title, message, and an "OK" button.
 *
 * The pop-up is modal. The callback is invoked when the user presses "OK". The underlying
 * view's button handlers must be re-registered in the callback.
 *
 * @param title The title of the alert box.
 * @param message The main text content of the alert.
 * @param cb The callback function to be executed when "OK" is pressed. Can be NULL.
 * @param user_data A user-defined pointer to be passed to the callback for context.
 */
void popup_manager_show_alert(const char* title, const char* message, popup_callback_t cb, void* user_data);

/**
 * @brief Shows a confirmation pop-up with two customizable action buttons.
 *
 * The pop-up is modal. The callback is invoked with the corresponding result
 * when the user presses either of the buttons. The underlying view's button
 * handlers must be re-registered in the callback.
 *
 * @param title The title of the confirmation box.
 * @param message The main text content of the confirmation.
 * @param primary_btn_text Text for the primary action button (e.g., "Confirm").
 * @param secondary_btn_text Text for the secondary action button (e.g., "Cancel").
 * @param cb The callback function to be executed with the result. Must not be NULL.
 * @param user_data A user-defined pointer to be passed to the callback for context.
 */
void popup_manager_show_confirmation(const char* title, const char* message, const char* primary_btn_text, const char* secondary_btn_text, popup_callback_t cb, void* user_data);

/**
 * @brief Shows a non-interactive "Loading..." pop-up with a spinner.
 *
 * This pop-up is modal but does not accept any input. It must be closed
 * programmatically by calling `popup_manager_hide_loading()`.
 *
 * @param message The message to display below the spinner (e.g., "Loading...").
 */
void popup_manager_show_loading(const char* message);

/**
 * @brief Hides the "Loading..." pop-up.
 *
 * Does nothing if the loading pop-up is not currently visible. Hiding the popup
 * does NOT re-enable the background view's buttons; that must be done manually.
 */
void popup_manager_hide_loading(void);

/**
 * @brief Checks if a modal popup (alert, confirmation, or loading) is currently active.
 * @return true if a popup is visible, false otherwise.
 */
bool popup_manager_is_active(void);

#ifdef __cplusplus
}
#endif

#endif // POPUP_MANAGER_H