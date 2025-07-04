#ifndef POMODORO_TIMER_COMPONENT_H
#define POMODORO_TIMER_COMPONENT_H

#include "lvgl.h"
#include "pomodoro_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback type for when the timer finishes or is cancelled.
typedef void (*pomodoro_exit_callback_t)(void);

/**
 * @brief Creates the Pomodoro timer component (the running screen).
 *
 * @param parent The parent LVGL object.
 * @param settings The settings for the session (work/break times, etc.).
 * @param on_exit_cb The callback function to execute when the timer is stopped or finishes.
 * @return lv_obj_t* Pointer to the main container of the component.
 */
lv_obj_t* pomodoro_timer_component_create(lv_obj_t* parent, const pomodoro_settings_t settings, pomodoro_exit_callback_t on_exit_cb);

#ifdef __cplusplus
}
#endif

#endif // POMODORO_TIMER_COMPONENT_H