#ifndef AUDIO_VISUALIZER_H
#define AUDIO_VISUALIZER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief The maximum number of bars the visualizer can display. */
#define AUDIO_VISUALIZER_MAX_BARS 32

/**
 * @brief Creates an audio visualizer widget using an LVGL canvas.
 * 
 * @param parent The parent LVGL object on which to create the visualizer.
 * @param bar_count The number of vertical bars to display (must be <= AUDIO_VISUALIZER_MAX_BARS).
 * @return A pointer to the created visualizer container object.
 */
lv_obj_t* audio_visualizer_create(lv_obj_t* parent, uint8_t bar_count);

/**
 * @brief Updates the height values of the visualizer's bars.
 * This function should be called repeatedly with new audio data to create the animation.
 * 
 * @param visualizer_cont A pointer to the visualizer container object returned by `audio_visualizer_create`.
 * @param values An array of `uint8_t` values (0-255), where each value represents the height of a bar. The array size must match the `bar_count` at creation.
 */
void audio_visualizer_set_values(lv_obj_t* visualizer_cont, uint8_t* values);

/**
 * @brief Sets a single solid color for all bars, overriding the default gradient effect.
 *
 * @param visualizer_cont A pointer to the visualizer container object.
 * @param color The `lv_color_t` to use for the bars.
 */
void audio_visualizer_set_bar_color(lv_obj_t* visualizer_cont, lv_color_t color);


#ifdef __cplusplus
}
#endif

#endif // AUDIO_VISUALIZER_H