# WiFi Audio Streamer

## Description
This component handles real-time audio streaming from the I2S microphone over a TCP connection to a server. It is designed to run in a dedicated FreeRTOS task, managing the connection lifecycle and I2S data handling.

## Features
-   Streams audio from the I2S microphone (e.g., INMP441).
-   Connects to a TCP server with IP and port configured in `secret.h`.
-   Manages its own state (`IDLE`, `CONNECTING`, `STREAMING`, `ERROR`) for clear UI feedback.
-   Processes 32-bit I2S data into 16-bit samples for transmission.
-   Automatically retries connection if the server is unavailable or disconnects.
-   Provides a simple API to start, stop, and query status.

## How to Use

1.  **Configure Server Details:**
    Set the server IP and port in `secret.h`.
    ```c
    #define STREAMING_SERVER_IP "Your_PC_IP_Address"
    #define STREAMING_SERVER_PORT 8888
    ```

2.  **Initialize the Streamer:**
    Call this once at startup.
    ```cpp
    #include "controllers/wifi_streamer/wifi_streamer.h"
    wifi_streamer_init();
    ```

3.  **Start/Stop Streaming:**
    These functions control the streaming task.
    ```cpp
    // This will spawn the task, wait for WiFi, and then attempt to connect and stream.
    if (!wifi_streamer_start()) {
        // Handle error (e.g., already running)
    }

    // This signals the task to stop, disconnect, and clean up.
    wifi_streamer_stop();
    ```

4.  **Get Status for UI:**
    Poll these functions from a UI timer to show feedback to the user.
    ```cpp
    wifi_stream_state_t state = wifi_streamer_get_state();
    char status_msg[128];
    wifi_streamer_get_status_message(status_msg, sizeof(status_msg));
    
    // Update labels and buttons based on the state and message
    ```

## Dependencies
-   `wifi_manager` for network connectivity.
-   `lwip` for socket communication.
-   `driver/i2s_std` for microphone input.
-   `secret.h` for server configuration.
-   `config.h` for I2S pin definitions.