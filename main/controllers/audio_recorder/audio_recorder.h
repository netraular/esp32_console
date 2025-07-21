/**
 * @file audio_recorder.h
 * @brief Manages audio recording from an I2S microphone to a WAV file.
 *
 * This controller operates in a dedicated FreeRTOS task to prevent blocking
 * the main application. It handles I2S configuration, file writing, and
 * correctly formatting the WAV header upon completion.
 */
#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief States for the audio recorder.
 */
typedef enum {
    RECORDER_STATE_IDLE,      //!< Not recording.
    RECORDER_STATE_RECORDING, //!< Actively recording audio.
    RECORDER_STATE_SAVING,    //!< Stop requested, finalizing WAV header.
    RECORDER_STATE_CANCELLING,//!< Cancel requested, stopping and deleting the file.
    RECORDER_STATE_ERROR      //!< An error occurred (e.g., I2S read/write fail).
} audio_recorder_state_t;

/**
 * @brief Initializes the audio recorder manager. Must be called once at startup.
 */
void audio_recorder_init(void);

/**
 * @brief Starts recording audio to a specified WAV file in a dedicated task.
 *
 * @param filepath The full path of the .wav file to create on the filesystem.
 * @return true if the recording task was successfully started, false otherwise.
 */
bool audio_recorder_start(const char *filepath);

/**
 * @brief Stops the current recording and saves the file.
 * Signals the recording task to finalize the WAV header and terminate.
 */
void audio_recorder_stop(void);

/**
 * @brief Cancels the current recording and deletes the partially created file.
 */
void audio_recorder_cancel(void);

/**
 * @brief Gets the current state of the recorder.
 */
audio_recorder_state_t audio_recorder_get_state(void);

/**
 * @brief Gets the elapsed duration of the current recording in seconds.
 * @return Duration in seconds if recording, otherwise 0.
 */
uint32_t audio_recorder_get_duration_s(void);


#endif // AUDIO_RECORDER_H