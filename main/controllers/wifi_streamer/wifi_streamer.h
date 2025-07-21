/**
 * @file wifi_streamer.h
 * @brief Handles real-time audio streaming from the I2S microphone over TCP.
 *
 * This controller runs in a dedicated FreeRTOS task, managing the connection
 * lifecycle to a server and streaming I2S data.
 */
#ifndef WIFI_STREAMER_H
#define WIFI_STREAMER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief States for the audio streamer.
 */
typedef enum {
    WIFI_STREAM_STATE_IDLE,           //!< The streamer task is inactive.
    WIFI_STREAM_STATE_CONNECTING,     //!< Attempting to connect to the TCP server.
    WIFI_STREAM_STATE_CONNECTED_IDLE, //!< Connected, but waiting for a START command from server.
    WIFI_STREAM_STATE_STREAMING,      //!< Actively sending audio data.
    WIFI_STREAM_STATE_STOPPING,       //!< A stop has been requested, task is shutting down.
    WIFI_STREAM_STATE_ERROR           //!< An error occurred (e.g., connection fail, I2S error).
} wifi_stream_state_t;

/**
 * @brief Initializes the WiFi streamer module. Must be called once at startup.
 */
void wifi_streamer_init(void);

/**
 * @brief Starts the audio streaming task.
 * The task will wait for WiFi, then connect to the server specified in `secret.h`.
 * @return true if the task was started successfully, false if it was already running.
 */
bool wifi_streamer_start(void);

/**
 * @brief Signals the audio streaming task to stop gracefully.
 */
void wifi_streamer_stop(void);

/**
 * @brief Gets the current state of the streamer for UI feedback.
 */
wifi_stream_state_t wifi_streamer_get_state(void);

/**
 * @brief Gets a human-readable status message for UI display.
 * @param buffer A character buffer to store the message.
 * @param buffer_size The size of the provided buffer.
 */
void wifi_streamer_get_status_message(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // WIFI_STREAMER_H