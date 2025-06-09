# Audio Manager

## Description
This component manages audio playback of WAV files using the I2S peripheral. It is designed to run in a dedicated FreeRTOS task, allowing for non-blocking audio playback control from the main application or UI.

## Features
-   Plays `.wav` files from a filesystem (e.g., SD card).
-   Supports basic playback controls: Play, Pause, Resume, and Stop.
-   Provides robust volume control, mapping a user-facing 0-100% scale to a safe physical limit.
-   Exposes functions to get the current playback state, total duration, and progress.
-   Handles I2S driver initialization and teardown automatically based on the WAV file's properties.
-   Parses standard WAV headers to configure I2S parameters like sample rate and bit depth.
-   **Provides real-time audio data for a visualizer**: Processes the audio stream and sends peak data for **32 frequency bars** to a FreeRTOS queue, allowing for a detailed spectrum display in the UI.

## How to Use

1.  **Initialize the Manager:**
    Call this function once at application startup. It sets up the initial state.
    ```cpp
    #include "controllers/audio_manager/audio_manager.h"
    
    // In your main function or an initialization sequence
    audio_manager_init();
    ```

2.  **Play a File:**
    To start playback, provide the full path to a `.wav` file. This will create a new task to handle the audio stream.
    ```cpp
    if (sd_manager_is_mounted()) {
        const char* audio_file = "/sdcard/music/my_song.wav";
        if (!audio_manager_play(audio_file)) {
            // Handle error, e.g., task creation failed
        }
    }
    ```

3.  **Control Playback:**
    Use these functions to control the audio stream from your UI or event handlers.
    ```cpp
    // Pause the currently playing audio
    audio_manager_pause();
    
    // Resume paused audio
    audio_manager_resume();
    
    // Stop playback completely and clean up resources
    audio_manager_stop();
    ```

4.  **Control Volume:**
    Adjust the volume in predefined steps.
    ```cpp
    // Increase volume
    audio_manager_volume_up();
    
    // Decrease volume
    audio_manager_volume_down();
    ```

5.  **Get Status Information:**
    These functions are useful for updating a user interface (e.g., a progress bar or status label).
    ```cpp
    audio_player_state_t state = audio_manager_get_state();
    uint32_t duration_sec = audio_manager_get_duration_s();
    uint32_t progress_sec = audio_manager_get_progress_s();
    uint8_t volume_percent = audio_manager_get_volume(); // This is the internal physical value

    // Update UI based on the values...
    ```

6.  **Using the Audio Visualizer:**
    The manager processes audio data and sends peak values to a queue, which can be read by the UI to draw a spectrum visualizer.
    ```cpp
    #include "controllers/audio_manager/audio_manager.h"

    // In your UI update task or timer:
    QueueHandle_t viz_queue = audio_manager_get_visualizer_queue();
    visualizer_data_t viz_data;

    if (viz_queue != NULL) {
        // Try to receive new data without blocking. 
        // xQueueReceive or xQueuePeek can be used.
        if (xQueueReceive(viz_queue, &viz_data, 0) == pdPASS) {
            // New data is available in viz_data.bar_values
            // Update your LVGL visualizer widget here.
            // for (int i = 0; i < VISUALIZER_BAR_COUNT; i++) {
            //    update_bar(i, viz_data.bar_values[i]);
            // }
        }
    }
    ```

## Dependencies
-   Requires an I2S-compatible audio codec/amplifier (e.g., MAX98357A) connected to the I2S pins defined in `config.h`.
-   Depends on a mounted filesystem (like the `sd_card_manager`) to access audio files.

## Limitations
-   Currently supports only WAV file format (16-bit or 8-bit).
-   The WAV parser is basic and expects standard `fmt` and `data` chunks. It may not support all variations of the format.