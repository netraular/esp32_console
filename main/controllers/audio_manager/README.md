# Audio Manager

## Description
This component manages audio playback of WAV files using the I2S peripheral. It is designed to run in a dedicated FreeRTOS task, allowing for non-blocking audio playback control from the main application or UI. It incorporates robust error handling and advanced task synchronization for stable performance.

## Features
-   Plays `.wav` files from a filesystem (e.g., SD card).
-   Supports basic playback controls: Play, Pause, Resume, and Stop.
-   **Robust Volume Control:** Implements a safe volume limit. It intelligently maps a user-facing 0-100% scale to a pre-configured physical maximum (e.g., 20%), protecting the speaker from damage.
-   **Advanced State Management:** Exposes functions to get the current playback state (including `AUDIO_STATE_ERROR`), total duration, and progress.
-   **Error Handling:** Can detect playback errors (like an SD card being removed) and transition to an `AUDIO_STATE_ERROR` state, allowing the UI to react gracefully.
-   **Automatic I2S Configuration:** Initializes and tears down the I2S driver automatically based on the WAV file's properties (sample rate, bit depth).
-   **Real-time Audio Visualizer Data:** Processes the audio stream and sends peak data for **32 frequency bars** to a FreeRTOS queue, enabling a detailed spectrum display in the UI.

## How to Use

1.  **Initialize the Manager:**
    Call this function once at application startup.
    ```cpp
    #include "controllers/audio_manager/audio_manager.h"
    
    audio_manager_init();
    ```

2.  **Play a File:**
    To start playback, provide the full path to a `.wav` file.
    ```cpp
    if (!audio_manager_play("/sdcard/music/my_song.wav")) {
        // Handle error, e.g., previous task didn't stop in time
    }
    ```

3.  **Control Playback:**
    Use these functions from your UI or event handlers.
    ```cpp
    audio_manager_pause();
    audio_manager_resume();
    audio_manager_stop(); // Stops playback and cleans up resources
    ```

4.  **Control Volume:**
    Adjust the volume in predefined steps. The manager handles the mapping to the safe physical limit.
    ```cpp
    audio_manager_volume_up();
    audio_manager_volume_down();
    ```

5.  **Get Status Information:**
    These are essential for updating a user interface.
    ```cpp
    audio_player_state_t state = audio_manager_get_state();

    if (state == AUDIO_STATE_ERROR) {
        // The player stopped due to an error (e.g., SD card removed)
        // Update UI to show an error message and disable playback controls.
    }

    uint32_t duration_sec = audio_manager_get_duration_s();
    uint32_t progress_sec = audio_manager_get_progress_s();
    
    // Note: This returns the internal physical volume (e.g., 0-20).
    // The UI is responsible for scaling this to a 0-100% display value.
    uint8_t physical_volume = audio_manager_get_volume(); 
    ```

6.  **Using the Audio Visualizer:**
    Read the visualizer data from the queue provided by the manager.
    ```cpp
    #include "controllers/audio_manager/audio_manager.h"

    // In your UI update task or timer:
    QueueHandle_t viz_queue = audio_manager_get_visualizer_queue();
    visualizer_data_t viz_data;

    if (xQueueReceive(viz_queue, &viz_data, 0) == pdPASS) {
        // New data is in viz_data.bar_values, update your LVGL widget.
        // for (int i = 0; i < VISUALIZER_BAR_COUNT; i++) {
        //    update_bar(i, viz_data.bar_values[i]);
        // }
    }
    ```

## Dependencies
-   Requires an I2S-compatible audio codec/amplifier (e.g., MAX98357A).
-   Depends on a mounted filesystem (like the `sd_card_manager`).

## Limitations
-   Currently supports only WAV file format (16-bit or 8-bit PCM).
-   The WAV parser is basic and expects standard `fmt ` and `data` chunks.