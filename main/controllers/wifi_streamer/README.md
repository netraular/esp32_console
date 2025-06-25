# WiFi Audio Streamer

## Description
This component handles real-time audio streaming from the I2S microphone over a TCP connection to a server. It is designed to run in a dedicated FreeRTOS task.

## Features
-   Streams audio from the I2S microphone (e.g., INMP441).
-   Connects to a configurable TCP server.
-   Manages its own state (`IDLE`, `CONNECTING`, `STREAMING`, `ERROR`).
-   Processes 32-bit I2S data into 16-bit samples for transmission.
-   Provides a status API for UI feedback.

## How to Use

1.  **Initialize the Streamer:**
    Call this once at startup, after the WiFi manager is initialized.
    ```cpp
    #include "controllers/wifi_streamer/wifi_streamer.h"
    wifi_streamer_init();
    ```

2.  **Start/Stop Streaming:**
    These functions control the streaming task.
    ```cpp
    // This will spawn the task and attempt to connect and stream.
    if (!wifi_streamer_start()) {
        // Handle error (e.g., already running)
    }

    // This signals the task to stop and clean up.
    wifi_streamer_stop();
    ```

3.  **Get Status:**
    Poll these functions from the UI to show feedback.
    ```cpp
    wifi_stream_state_t state = wifi_streamer_get_state();
    char status_msg[128];
    wifi_streamer_get_status_message(status_msg, sizeof(status_msg));
    ```