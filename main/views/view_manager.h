#ifndef VIEW_MANAGER_H
#define VIEW_MANAGER_H

#include "lvgl.h"

// Enum para identificar cada vista de forma única
typedef enum {
    VIEW_ID_MENU,
    VIEW_ID_MIC_TEST,
    VIEW_ID_SPEAKER_TEST,
    VIEW_ID_COUNT // Número total de vistas
} view_id_t;

/**
 * @brief Inicializa el gestor de vistas y carga la vista inicial.
 */
void view_manager_init(void);

/**
 * @brief Carga una nueva vista.
 * Se encarga de destruir la vista actual, limpiar la pantalla y crear la nueva.
 * @param view_id El ID de la vista a cargar.
 */
void view_manager_load_view(view_id_t view_id);

#endif // VIEW_MANAGER_H