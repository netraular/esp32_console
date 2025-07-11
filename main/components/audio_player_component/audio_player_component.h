#ifndef AUDIO_PLAYER_COMPONENT_H
#define AUDIO_PLAYER_COMPONENT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback para notificar a la vista contenedora que el usuario quiere salir.
typedef void (*audio_player_exit_callback_t)(void);

/**
 * @brief Crea un componente de reproductor de audio de pantalla completa.
 * 
 * Este componente se encarga de la UI de reproducción, la interacción con
 * el audio_manager y el manejo de los botones de control.
 *
 * @param parent El objeto padre LVGL (normalmente la pantalla activa).
 * @param file_path La ruta completa del archivo .wav a reproducir.
 * @param on_exit Callback que se ejecuta cuando el usuario presiona Cancelar.
 * @return lv_obj_t* Un puntero al objeto contenedor principal del reproductor.
 */
lv_obj_t* audio_player_component_create(
    lv_obj_t* parent,
    const char* file_path,
    audio_player_exit_callback_t on_exit
);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_PLAYER_COMPONENT_H