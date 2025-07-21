/**
 * @file audio_visualizer.h
 * @brief A custom, reusable LVGL widget for displaying a bar-style audio visualizer.
 *
 * This component uses an LVGL Canvas for efficient drawing and is decoupled from any
 * specific audio source. It renders a configurable number of bars with a smooth
 * color gradient, automatically centered and spaced for a polished look.
 */
#ifndef AUDIO_VISUALIZER_H
#define AUDIO_VISUALIZER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief The maximum number of bars the visualizer can display. */
#define AUDIO_VISUALIZER_MAX_BARS 32

/**
 * @brief Creates an audio visualizer widget.
 *
 * @param parent The parent LVGL object to create the visualizer on.
 * @param bar_count The number of vertical bars to display (clamped to AUDIO_VISUALIZER_MAX_BARS).
 * @return A pointer to the created visualizer container object.
 */
lv_obj_t* audio_visualizer_create(lv_obj_t* parent, uint8_t bar_count);

/**
 * @brief Updates the height values of the visualizer's bars.
 *
 * Call this function repeatedly with new audio data to create the animation.
 * The component is optimized to only redraw if the values have changed.
 *
 * @param visualizer_cont A pointer to the container object returned by `audio_visualizer_create`.
 * @param values An array of `uint8_t` values (0-255). Each value represents a bar's height.
 *               The array size must match the `bar_count` provided at creation.
 */
void audio_visualizer_set_values(lv_obj_t* visualizer_cont, uint8_t* values);

/**
 * @brief Sets a single solid color for all bars, overriding the default gradient.
 *
 * @param visualizer_cont A pointer to the visualizer container object.
 * @param color The `lv_color_t` to use for all bars.
 */
void audio_visualizer_set_bar_color(lv_obj_t* visualizer_cont, lv_color_t color);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_VISUALIZER_H