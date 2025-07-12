#ifndef STT_MANAGER_H
#define STT_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback to notify the result of the transcription.
 *
 * @param success true if the transcription was successful.
 * @param result Pointer to the transcribed text or an error message. The receiver of this callback is responsible for freeing this buffer with `free()`.
 */
typedef void (*stt_result_callback_t)(bool success, char* result);

/**
 * @brief Initializes the Speech-to-Text manager.
 */
void stt_manager_init(void);

/**
 * @brief Starts the transcription of an audio file in a background task.
 * The task handles WiFi/Time sync checks, file reading, and the HTTP request.
 *
 * @param file_path Full path of the .wav file to transcribe.
 * @param cb The callback that will be executed upon completion or failure.
 * @return true if the transcription task was successfully started, false otherwise.
 */
bool stt_manager_transcribe(const char* file_path, stt_result_callback_t cb);

#ifdef __cplusplus
}
#endif

#endif // STT_MANAGER_H