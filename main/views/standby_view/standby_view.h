#ifndef STANDBY_VIEW_H
#define STANDBY_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Crea la vista de standby (pantalla de reposo).
 * Muestra la hora actual y espera la entrada del usuario para ir al menú principal.
 *
 * @param parent El objeto padre LVGL sobre el que se creará la vista.
 */
void standby_view_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif // STANDBY_VIEW_H