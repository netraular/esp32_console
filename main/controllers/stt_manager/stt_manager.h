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
#include <string>
#include <functional>

/**
 * @brief Callback to notify the result of the transcription.
 *
 * This callback is memory-safe. The `result` string is passed by const reference
 * and is managed internally by the stt_manager.
 *
 * @param success true if the transcription was successful.
 * @param result The transcribed text (on success) or an error message (on failure).
 */
typedef std::function<void(bool success, const std::string& result)> stt_result_callback_t;

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
 * @return true if the transcription task was successfully started, false otherwise.
 */
bool stt_manager_transcribe(const std::string& file_path, stt_result_callback_t cb);

#endif // STT_MANAGER_H