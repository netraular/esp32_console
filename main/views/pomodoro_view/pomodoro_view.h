#ifndef POMODORO_VIEW_H
#define POMODORO_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Crea la vista del reloj Pomodoro y sus controles.
 *
 * @param parent El objeto padre LVGL sobre el que se creará la vista.
 */
void pomodoro_view_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif // POMODORO_VIEW_H