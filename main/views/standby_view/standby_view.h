#ifndef STANDBY_VIEW_H
#define STANDBY_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates the standby view (idle screen).
 * Displays the current time and waits for user input to navigate to the main menu.
 *
 * @param parent The parent LVGL object on which the view will be created.
 */
void standby_view_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif // STANDBY_VIEW_H