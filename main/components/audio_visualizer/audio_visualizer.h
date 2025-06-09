#ifndef AUDIO_VISUALIZER_H
#define AUDIO_VISUALIZER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- [CAMBIO] Aumentamos el número máximo de barras a 32 ---
#define AUDIO_VISUALIZER_MAX_BARS 32

/**
 * @brief Crea un objeto de visualizador de audio.
 * 
 * @param parent El objeto padre sobre el que se creará el visualizador.
 * @param bar_count El número de barras a mostrar (debe ser <= AUDIO_VISUALIZER_MAX_BARS).
 * @return lv_obj_t* Puntero al objeto visualizador creado.
 */
lv_obj_t* audio_visualizer_create(lv_obj_t* parent, uint8_t bar_count);

/**
 * @brief Actualiza los valores de las barras del visualizador.
 * 
 * @param visualizer Puntero al objeto visualizador.
 * @param values Un array de valores (0-255) para la altura de cada barra.
 */
void audio_visualizer_set_values(lv_obj_t* visualizer, uint8_t* values);

/**
 * @brief Establece el color de las barras.
 *
 * @param visualizer Puntero al objeto visualizador.
 * @param color El color a usar para las barras.
 */
void audio_visualizer_set_bar_color(lv_obj_t* visualizer, lv_color_t color);


#ifdef __cplusplus
}
#endif

#endif // AUDIO_VISUALIZER_H