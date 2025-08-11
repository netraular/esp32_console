#ifndef MEMORY_MONITOR_COMPONENT_H
#define MEMORY_MONITOR_COMPONENT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates a memory monitor widget.
 *
 * This component displays the current usage of internal RAM and PSRAM.
 * It automatically updates the information at a fixed interval using an
 * internal LVGL timer and cleans up its own resources upon deletion.
 *
 * @param parent The parent LVGL object on which the monitor will be created.
 * @return A pointer to the main container object of the memory monitor.
 */
lv_obj_t* memory_monitor_create(lv_obj_t* parent);

#ifdef __cplusplus
}
#endif

#endif // MEMORY_MONITOR_COMPONENT_H