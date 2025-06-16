#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Audio player states
typedef enum {
    AUDIO_STATE_STOPPED, // Playback is stopped or has finished.
    AUDIO_STATE_PLAYING, // Playback is active.
    AUDIO_STATE_PAUSED,  // Playback is paused.
    AUDIO_STATE_ERROR    // An error occurred during playback (e.g., file not found, SD card removed).
} audio_player_state_t;

/**
 * @brief Initializes the audio manager.
 */
void audio_manager_init(void);

/**
 * @brief Starts playback of a WAV file in a new task.
 * @param filepath Full path to the .wav file.
 * @return true if the playback task was started, false on error.
 */
bool audio_manager_play(const char *filepath);

/**
 * @brief Pauses the current audio playback.
 */
void audio_manager_pause(void);

/**
 * @brief Resumes audio playback if paused.
 */
void audio_manager_resume(void);

/**
 * @brief Stops audio playback and releases resources.
 */
void audio_manager_stop(void);

/**
 * @brief Gets the current player state.
 * @return The current audio_player_state_t.
 */
audio_player_state_t audio_manager_get_state(void);

/**
 * @brief Gets the total duration of the current song in seconds.
 * @return Duration in seconds, or 0 if nothing is playing.
 */
uint32_t audio_manager_get_duration_s(void);

/**
 * @brief Gets the current playback progress in seconds.
 * @return Progress in seconds.
 */
uint32_t audio_manager_get_progress_s(void);

/**
 * @brief Increases volume by a predefined step.
 */
void audio_manager_volume_up(void);

/**
 * @brief Decreases volume by a predefined step.
 */
void audio_manager_volume_down(void);

/**
 * @brief Gets the current physical volume level.
 * @return The volume as a percentage of the configured maximum (e.g., 0-25).
 */
uint8_t audio_manager_get_volume(void);


// Number of bars for the audio visualizer
#define VISUALIZER_BAR_COUNT 32

// Data structure for visualizer data sent via queue
typedef struct {
    uint8_t bar_values[VISUALIZER_BAR_COUNT];
} visualizer_data_t;

/**
 * @brief Gets the handle of the visualizer queue.
 * The UI uses this queue to receive bar data for rendering.
 * @return Handle to the queue, or NULL if not initialized.
 */
QueueHandle_t audio_manager_get_visualizer_queue(void);

#endif // AUDIO_MANAGER_H