/**
 * @file audio_player_component.h
 * @brief A self-contained, full-screen LVGL component for playing WAV audio files.
 *
 * Provides a complete user interface including playback controls (Play/Pause, Volume),
 * progress display, song title, and a real-time audio visualizer. It handles its own
 * button inputs and playback state via the audio_manager.
 */
#ifndef AUDIO_PLAYER_COMPONENT_H
#define AUDIO_PLAYER_COMPONENT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function to notify the parent that the user has exited the player.
 * @param user_data A user-provided pointer, passed during component creation.
 */
typedef void (*audio_player_exit_callback_t)(void* user_data);

/**
 * @brief Creates and displays a full-screen audio player, and starts playback immediately.
 *
 * This function builds the entire UI and takes control of the buttons. When the user exits,
 * the `on_exit` callback is invoked. The parent view is then responsible for cleaning up the
 * player UI (e.g., by calling `lv_obj_clean(lv_screen_active())` which triggers the
 * component's internal cleanup).
 *
 * @param parent The parent LVGL object (typically the active screen).
 * @param file_path The full path of the .wav file on the filesystem to play.
 * @param on_exit Callback function executed when the user presses the 'Cancel' button.
 * @param exit_cb_user_data A user-defined pointer passed to the `on_exit` callback, useful for providing context (like `this`).
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