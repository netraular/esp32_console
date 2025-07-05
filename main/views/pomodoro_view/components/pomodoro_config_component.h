#ifndef POMODORO_CONFIG_COMPONENT_H
#define POMODORO_CONFIG_COMPONENT_H

#include "lvgl.h"
#include "pomodoro_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback type for when the user clicks the "START" button.
// It passes the final settings back to the parent view.
typedef void (*pomodoro_start_callback_t)(const pomodoro_settings_t settings);

/**
 * @brief Creates the Pomodoro configuration component.
 *
 * @param parent The parent LVGL object.
 * @param initial_settings The default settings to display.
 * @param on_start_cb The callback function to execute when "START" is pressed.
 * @return lv_obj_t* Pointer to the main container of the component.
 */
lv_obj_t* pomodoro_config_component_create(lv_obj_t* parent, const pomodoro_settings_t initial_settings, pomodoro_start_callback_t on_start_cb);

#ifdef __cplusplus
}
#endif

#endif // POMODORO_CONFIG_COMPONENT_H