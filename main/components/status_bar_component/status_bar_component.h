#ifndef STATUS_BAR_COMPONENT_H
#define STATUS_BAR_COMPONENT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Crea un componente de barra de estado.
 *
 * @param parent El objeto padre LVGL sobre el que se creará la barra de estado.
 * @return Un puntero al objeto contenedor principal de la barra de estado.
 */
lv_obj_t* status_bar_create(lv_obj_t* parent);

/**
 * @brief Fuerza una actualización inmediata de los indicadores de volumen en la barra de estado.
 * 
 * Llama a esta función después de cambiar el volumen para que la UI se actualice
 * instantáneamente, sin esperar al ciclo de refresco periódico.
 */
void status_bar_update_volume_display(void);


#ifdef __cplusplus
}
#endif

#endif // STATUS_BAR_COMPONENT_H