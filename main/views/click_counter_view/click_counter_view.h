#ifndef CLICK_COUNTER_VIEW_H
#define CLICK_COUNTER_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Crea la interfaz de usuario para la vista del contador de clics.
 * 
 * @param parent El objeto padre sobre el que se creará la interfaz (normalmente la pantalla activa).
 */
void click_counter_view_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif // CLICK_COUNTER_VIEW_H