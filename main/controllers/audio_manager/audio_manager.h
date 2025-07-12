#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief States for the audio player.
 */
typedef enum {
    AUDIO_STATE_STOPPED, // Playback is stopped or has finished.
    AUDIO_STATE_PLAYING, // Playback is active.
    AUDIO_STATE_PAUSED,  // Playback is paused.
    AUDIO_STATE_ERROR    // An error occurred during playback (e.g., file not found, SD card removed).
} audio_player_state_t;

/**
 * @brief Initializes the audio manager, including its volume control mutex and task synchronization semaphores.
 * Must be called once at application startup.
 */
void audio_manager_init(void);

/**
 * @brief Starts playback of a WAV file in a new, dedicated FreeRTOS task.
 * If another track is playing, it will be stopped first.
 * @param filepath Full path to the .wav file on the filesystem (e.g., "/sdcard/sound.wav").
 * @return true if the playback task was successfully created, false on error (e.g., another task is still cleaning up).
 */
bool audio_manager_play(const char *filepath);

/**
 * @brief Pauses the current audio playback. Has no effect if not playing.
 */
void audio_manager_pause(void);

/**
 * @brief Resumes audio playback if it was paused. Has no effect otherwise.
 */
void audio_manager_resume(void);

/**
 * @brief Stops audio playback, terminates the playback task, and releases its resources.
 * This function waits for the task to confirm termination.
 */
void audio_manager_stop(void);

/**
 * @brief Gets the current state of the audio player.
 * @return The current audio_player_state_t.
 */
audio_player_state_t audio_manager_get_state(void);

/**
 * @brief Gets the total duration of the currently loaded song in seconds.
 * @return Duration in seconds, or 0 if no song is loaded or duration is unknown.
 */
uint32_t audio_manager_get_duration_s(void);

/**
 * @brief Gets the current playback progress of the song in seconds.
 * @return Current progress in seconds.
 */
uint32_t audio_manager_get_progress_s(void);

/**
 * @brief Increases volume by a predefined step, mapping a 0-100 UI scale to the safe physical range.
 */
void audio_manager_volume_up(void);

/**
 * @brief Decreases volume by a predefined step, mapping a 0-100 UI scale to the safe physical range.
 */
void audio_manager_volume_down(void);

/**
 * @brief Gets the current physical volume level.
 * This is the raw value (0-100) that is applied, which is capped by MAX_VOLUME_PERCENTAGE.
 * For UI display, this value should be scaled.
 * @return The volume as a percentage of the configured maximum (e.g., 0-20).
 */
uint8_t audio_manager_get_volume(void);


// Number of bars for the audio visualizer
#define VISUALIZER_BAR_COUNT 32

/**
 * @brief Data structure for visualizer data sent via queue.
 */
typedef struct {
    uint8_t bar_values[VISUALIZER_BAR_COUNT];
} visualizer_data_t;

/**
 * @brief Gets the handle of the visualizer data queue.
 * The UI can use this queue to receive audio spectrum data for rendering. The queue is an overwrite queue (size 1).
 * @return Handle to the queue, or NULL if not initialized.
 */
QueueHandle_t audio_manager_get_visualizer_queue(void);

#endif // AUDIO_MANAGER_H