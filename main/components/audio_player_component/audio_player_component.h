#ifndef AUDIO_PLAYER_COMPONENT_H
#define AUDIO_PLAYER_COMPONENT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function type to notify the parent view that the user wants to exit the player.
 * @param user_data A user-provided pointer, passed during component creation.
 */
typedef void (*audio_player_exit_callback_t)(void* user_data);

/**
 * @brief Creates a full-screen audio player component.
 *
 * This component builds a complete UI for audio playback, including a title,
 * volume indicator, progress slider, and audio visualizer. It takes full control
 * of the relevant buttons and manages the audio playback lifecycle via the audio_manager.
 * It begins playing the specified file immediately upon creation.
 *
 * @param parent The parent LVGL object (typically the active screen).
 * @param file_path The full path of the .wav file to play.
 * @param on_exit Callback function that is executed when the user presses the Cancel button to exit.
 * @param exit_cb_user_data A user-defined pointer that will be passed to the on_exit callback.
 * @return A pointer to the main container object of the audio player UI.
 */
lv_obj_t* audio_player_component_create(
    lv_obj_t* parent,
    const char* file_path,
    audio_player_exit_callback_t on_exit,
    void* exit_cb_user_data
);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_PLAYER_COMPONENT_H