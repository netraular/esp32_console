#ifndef WIFI_STREAMER_H
#define WIFI_STREAMER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// States for the streamer
typedef enum {
    WIFI_STREAM_STATE_IDLE,
    WIFI_STREAM_STATE_CONNECTING,
    WIFI_STREAM_STATE_STREAMING,
    WIFI_STREAM_STATE_STOPPING,
    WIFI_STREAM_STATE_ERROR
} wifi_stream_state_t;

/**
 * @brief Initializes the WiFi streamer module.
 */
void wifi_streamer_init(void);

/**
 * @brief Starts the audio streaming task.
 * The task will attempt to connect to the server and stream audio.
 * @return true if the task was started, false if it was already running.
 */
bool wifi_streamer_start(void);

/**
 * @brief Stops the audio streaming task.
 */
void wifi_streamer_stop(void);

/**
 * @brief Gets the current state of the streamer.
 * @return The current wifi_stream_state_t.
 */
wifi_stream_state_t wifi_streamer_get_state(void);

/**
 * @brief Gets a human-readable status message.
 * @param buffer The buffer to store the message.
 * @param buffer_size The size of the provided buffer.
 */
void wifi_streamer_get_status_message(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // WIFI_STREAMER_H