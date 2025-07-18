#ifndef POMODORO_VIEW_H
#define POMODORO_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates the Pomodoro Clock view and its controls.
 *
 * @param parent The parent LVGL object on which the view will be created.
 */
void pomodoro_view_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif // POMODORO_VIEW_H