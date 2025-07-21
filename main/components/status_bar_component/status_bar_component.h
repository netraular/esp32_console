/**
 * @file status_bar_component.h
 * @brief A component for displaying system status like time, WiFi, and volume.
 *
 * This component creates a bar at the top of the screen and automatically
 * updates its state via an internal timer.
 */
#ifndef STATUS_BAR_COMPONENT_H
#define STATUS_BAR_COMPONENT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates a status bar component.
 *
 * The bar displays WiFi status, date/time (once synced), and volume level.
 * It manages its own update timer.
 *
 * @param parent The parent LVGL object on which the status bar will be created.
 * @return A pointer to the main container object of the status bar.
 */
lv_obj_t* status_bar_create(lv_obj_t* parent);

/**
 * @brief Forces an immediate update of the volume indicators in the status bar.
 *
 * Call this function after programmatically changing the volume so the UI updates
 * instantly, without waiting for the status bar's periodic refresh cycle.
 */
void status_bar_update_volume_display(void);

#ifdef __cplusplus
}
#endif

#endif // STATUS_BAR_COMPONENT_H