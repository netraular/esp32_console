/**
 * @file audio_manager.h
 * @brief Manages audio playback of WAV files using the I2S peripheral.
 *
 * This controller runs playback in a dedicated FreeRTOS task, providing non-blocking
 * control. It features safe volume limits, dynamic frequency filtering to reduce
 * distortion on small speakers, and provides data for a real-time visualizer.
 */
#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief Maximum number of bars for the audio visualizer data.
 * Must be kept in sync with the visualizer component.
 */
#define VISUALIZER_BAR_COUNT 32

/**
 * @brief States for the audio player.
 */
typedef enum {
    AUDIO_STATE_STOPPED, //!< Playback is stopped or has finished.
    AUDIO_STATE_PLAYING, //!< Playback is active.
    AUDIO_STATE_PAUSED,  //!< Playback is paused.
    AUDIO_STATE_ERROR    //!< An error occurred (e.g., file not found, SD card removed).
} audio_player_state_t;

/**
 * @brief Data structure for visualizer data sent via queue.
 */
typedef struct {
    uint8_t bar_values[VISUALIZER_BAR_COUNT];
} visualizer_data_t;


/**
 * @brief Initializes the audio manager. Must be called once at startup.
 */
void audio_manager_init(void);

/**
 * @brief Starts playback of a WAV file in a new FreeRTOS task.
 * If another track is playing, it will be stopped first.
 * @param filepath Full path to the .wav file on the filesystem.
 * @return true if the playback task was created, false on error.
 */
bool audio_manager_play(const char *filepath);

/** @brief Pauses the current audio playback. */
void audio_manager_pause(void);

/** @brief Resumes audio playback if it was paused. */
void audio_manager_resume(void);

/** @brief Stops audio playback and terminates the playback task. */
void audio_manager_stop(void);

/** @brief Gets the current state of the audio player. */
audio_player_state_t audio_manager_get_state(void);

/** @brief Gets the total duration of the current song in seconds. */
uint32_t audio_manager_get_duration_s(void);

/** @brief Gets the current playback progress of the song in seconds. */
uint32_t audio_manager_get_progress_s(void);

/** @brief Increases volume by one step, respecting the safe physical range. */
void audio_manager_volume_up(void);

/** @brief Decreases volume by one step. */
void audio_manager_volume_down(void);

/**
 * @brief Gets the current physical volume level (0-100), capped by `MAX_VOLUME_PERCENTAGE`.
 * @return The raw physical volume percentage being applied.
 */
uint8_t audio_manager_get_volume(void);

/**
 * @brief Sets the physical volume directly, bypassing the UI-step logic.
 * @param percentage The raw physical volume to set (0-100).
 */
void audio_manager_set_volume_physical(uint8_t percentage);

/**
 * @brief Gets the handle of the visualizer data queue.
 *
 * The UI can use this queue to receive audio spectrum data for rendering.
 * It is a size-1 overwrite queue, so only the latest data is available.
 *
 * @return Handle to the queue, or NULL if not initialized.
 */
QueueHandle_t audio_manager_get_visualizer_queue(void);

#endif // AUDIO_MANAGER_H