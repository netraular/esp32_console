#ifndef MENU_VIEW_H
#define MENU_VIEW_H

#include "lvgl.h"

/**
 * @brief Creates the menu view UI and registers its button handlers.
 * @param parent The parent object on which the UI will be created (usually the active screen).
 */
void menu_view_create(lv_obj_t *parent);

#endif // MENU_VIEW_H