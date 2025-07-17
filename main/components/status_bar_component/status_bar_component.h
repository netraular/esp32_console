#ifndef STATUS_BAR_COMPONENT_H
#define STATUS_BAR_COMPONENT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates a status bar component.
 *
 * @param parent The parent LVGL object on which the status bar will be created.
 * @return A pointer to the main container object of the status bar.
 */
lv_obj_t* status_bar_create(lv_obj_t* parent);

/**
 * @brief Forces an immediate update of the volume indicators in the status bar.
 * 
 * Call this function after changing the volume so that the UI updates
 * instantly, without waiting for the periodic refresh cycle.
 */
void status_bar_update_volume_display(void);


#ifdef __cplusplus
}
#endif

#endif // STATUS_BAR_COMPONENT_H