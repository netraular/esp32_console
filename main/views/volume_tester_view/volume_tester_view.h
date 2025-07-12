#ifndef VOLUME_TESTER_VIEW_H
#define VOLUME_TESTER_VIEW_H

#include "lvgl.h"

/**
 * @brief Creates the volume tester view UI and registers its button handlers.
 * @param parent The parent object on which the UI will be created (usually the active screen).
 */
void volume_tester_view_create(lv_obj_t *parent);

#endif // VOLUME_TESTER_VIEW_H