#ifndef CLICK_COUNTER_VIEW_H
#define CLICK_COUNTER_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates the user interface for the click counter view.
 * 
 * @param parent The parent object on which the UI will be created (usually the active screen).
 */
void click_counter_view_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif // CLICK_COUNTER_VIEW_H