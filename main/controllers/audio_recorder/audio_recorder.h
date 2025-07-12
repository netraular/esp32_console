#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief States for the audio recorder.
 */
typedef enum {
    RECORDER_STATE_IDLE,      // Not recording.
    RECORDER_STATE_RECORDING, // Actively recording audio.
    RECORDER_STATE_SAVING,    // Stop requested, finalizing WAV header.
    RECORDER_STATE_CANCELLING,// Cancel requested, stopping and deleting the file.
    RECORDER_STATE_ERROR      // An error occurred (e.g., I2S read fail, file write fail).
} audio_recorder_state_t;

/**
 * @brief Initializes the audio recorder manager. Must be called once at application startup.
 */
void audio_recorder_init(void);

/**
 * @brief Starts recording audio to a specified WAV file in a dedicated FreeRTOS task.
 * 
 * @param filepath The full path to the .wav file to create on the filesystem.
 * @return true if the recording task was successfully started, false otherwise (e.g., already recording).
 */
bool audio_recorder_start(const char *filepath);

/**
 * @brief Stops the current audio recording and saves the file.
 * This signals the recording task to finalize the file (write the correct header) and terminate.
 */
void audio_recorder_stop(void);

/**
 * @brief Cancels the current audio recording and deletes the partially created file.
 * This signals the recording task to stop and discard all recorded data.
 */
void audio_recorder_cancel(void);

/**
 * @brief Gets the current state of the recorder.
 * @return The current audio_recorder_state_t.
 */
audio_recorder_state_t audio_recorder_get_state(void);

/**
 * @brief Gets the elapsed duration of the current recording in seconds.
 * @return Duration in seconds if recording, otherwise 0.
 */
uint32_t audio_recorder_get_duration_s(void);


#endif // AUDIO_RECORDER_H