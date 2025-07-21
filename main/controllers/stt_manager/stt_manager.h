/**
 * @file stt_manager.h
 * @brief Manages audio transcription using the remote Groq Speech-to-Text API.
 *
 * This controller performs API requests in a dedicated FreeRTOS task to avoid
 * blocking the UI. It handles HTTPS communication, multipart/form-data creation,
 * and reports results via a callback.
 */
#ifndef STT_MANAGER_H
#define STT_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback to notify the result of the transcription.
 *
 * @warning The receiver of this callback is responsible for freeing the `result`
 *          buffer with `free()` regardless of success or failure.
 *
 * @param success true if the transcription was successful.
 * @param result Pointer to the transcribed text (on success) or an error message (on failure).
 * @param user_data A user-provided pointer, passed during the `stt_manager_transcribe` call.
 */
typedef void (*stt_result_callback_t)(bool success, char* result, void* user_data);

/**
 * @brief Initializes the Speech-to-Text manager.
 */
void stt_manager_init(void);

/**
 * @brief Starts the transcription of an audio file in a background task.
 *
 * The task handles checking for WiFi/Time sync, reading the file, and performing
 * the HTTP request. The result is delivered via the provided callback.
 *
 * @param file_path Full path of the .wav file to transcribe.
 * @param cb The callback that will be executed upon completion or failure.
 * @param user_data A user-defined pointer that will be passed to the callback for context.
 * @return true if the transcription task was successfully started, false otherwise.
 */
bool stt_manager_transcribe(const char* file_path, stt_result_callback_t cb, void* user_data);

#ifdef __cplusplus
}
#endif

#endif // STT_MANAGER_H