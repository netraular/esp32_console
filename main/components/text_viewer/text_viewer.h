#ifndef TEXT_VIEWER_H
#define TEXT_VIEWER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback para notificar cuando el usuario quiere salir del visor
typedef void (*text_viewer_exit_callback_t)(void);

/**
 * @brief Crea un componente de visor de texto de pantalla completa.
 *
 * El componente toma posesión del puntero 'content' y lo liberará
 * automáticamente con free() cuando el visor sea destruido.
 *
 * @param parent El objeto padre LVGL (normalmente la pantalla activa).
 * @param title El título a mostrar en la parte superior.
 * @param content El contenido de texto a mostrar. Debe ser un buffer de memoria dinámica (malloc).
 * @param on_exit Callback que se ejecuta cuando el usuario presiona el botón de cancelar.
 * @return lv_obj_t* Un puntero al objeto contenedor principal del visor.
 */
lv_obj_t* text_viewer_create(lv_obj_t* parent, const char* title, char* content, text_viewer_exit_callback_t on_exit);

/**
 * @brief Destruye el componente visor de texto.
 *
 * Simplemente llama a lv_obj_del en el objeto del visor. La limpieza de la
 * memoria del contenido se maneja a través de un evento LV_EVENT_DELETE.
 *
 * @param viewer El objeto contenedor del visor devuelto por text_viewer_create.
 */
void text_viewer_destroy(lv_obj_t* viewer);


#ifdef __cplusplus
}
#endif

#endif // TEXT_VIEWER_H