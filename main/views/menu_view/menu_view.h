#ifndef MENU_VIEW_H
#define MENU_VIEW_H

#include "lvgl.h"

/**
 * @brief Crea la interfaz de usuario de la vista del menú y registra sus handlers de botón.
 * @param parent El objeto padre sobre el que se creará la UI (normalmente, la pantalla activa).
 */
void menu_view_create(lv_obj_t *parent);

#endif // MENU_VIEW_H