#ifndef AUDIO_VISUALIZER_H
#define AUDIO_VISUALIZER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// The maximum number of bars the visualizer can display.
#define AUDIO_VISUALIZER_MAX_BARS 32

/**
 * @brief Creates an audio visualizer widget.
 * 
 * @param parent The parent object on which to create the visualizer.
 * @param bar_count The number of bars to display (must be <= AUDIO_VISUALIZER_MAX_BARS).
 * @return lv_obj_t* Pointer to the created visualizer container object.
 */
lv_obj_t* audio_visualizer_create(lv_obj_t* parent, uint8_t bar_count);

/**
 * @brief Updates the height values of the visualizer bars.
 * 
 * @param visualizer_cont Pointer to the visualizer container object.
 * @param values An array of values (0-255) for the height of each bar.
 */
void audio_visualizer_set_values(lv_obj_t* visualizer_cont, uint8_t* values);

/**
 * @brief Sets a solid color for all bars, overriding the default gradient.
 *
 * @param visualizer_cont Pointer to the visualizer container object.
 * @param color The color to use for the bars.
 */
void audio_visualizer_set_bar_color(lv_obj_t* visualizer_cont, lv_color_t color);


#ifdef __cplusplus
}
#endif

#endif // AUDIO_VISUALIZER_H