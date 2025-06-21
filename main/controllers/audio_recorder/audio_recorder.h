#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H

#include <stdbool.h>
#include <stdint.h>

// Audio recorder states
typedef enum {
    RECORDER_STATE_IDLE,    // Not recording
    RECORDER_STATE_RECORDING, // Actively recording
    RECORDER_STATE_SAVING,    // Finishing up, writing final header
    RECORDER_STATE_ERROR      // An error occurred
} audio_recorder_state_t;

/**
 * @brief Initializes the audio recorder manager.
 */
void audio_recorder_init(void);

/**
 * @brief Starts recording audio to a specified WAV file.
 * This function creates a new task to handle the recording process.
 * 
 * @param filepath The full path to the .wav file to create.
 * @return true if the recording task was successfully started, false otherwise.
 */
bool audio_recorder_start(const char *filepath);

/**
 * @brief Stops the current audio recording.
 * Signals the recording task to finalize the file and terminate.
 */
void audio_recorder_stop(void);

/**
 * @brief Gets the current recorder state.
 * @return The current audio_recorder_state_t.
 */
audio_recorder_state_t audio_recorder_get_state(void);

/**
 * @brief Gets the duration of the current recording in seconds.
 * @return Duration in seconds.
 */
uint32_t audio_recorder_get_duration_s(void);


#endif // AUDIO_RECORDER_H