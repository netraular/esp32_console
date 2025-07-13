#ifndef STATUS_BAR_COMPONENT_H
#define STATUS_BAR_COMPONENT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Crea un componente de barra de estado.
 *
 * La barra de estado se actualiza automáticamente cada segundo y muestra:
 * - Estado del WiFi (izquierda)
 * - Nivel de volumen (izquierda)
 * - Hora y fecha actual (derecha)
 *
 * El componente gestiona su propio timer y se limpia automáticamente cuando su
 * objeto padre es eliminado.
 *
 * @param parent El objeto padre LVGL sobre el que se creará la barra de estado.
 * @return Un puntero al objeto contenedor principal de la barra de estado.
 */
lv_obj_t* status_bar_create(lv_obj_t* parent);

#ifdef __cplusplus
}
#endif

#endif // STATUS_BAR_COMPONENT_H