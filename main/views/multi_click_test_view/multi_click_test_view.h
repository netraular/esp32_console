#ifndef MULTI_CLICK_TEST_VIEW_H
#define MULTI_CLICK_TEST_VIEW_H

#include "lvgl.h"

/**
 * @brief Creates the user interface for the multi-click button test view.
 * This view demonstrates single/double click, long press, press down/up events.
 * @param parent The parent object on which to create the UI.
 */
void multi_click_test_view_create(lv_obj_t *parent);

#endif // MULTI_CLICK_TEST_VIEW_H