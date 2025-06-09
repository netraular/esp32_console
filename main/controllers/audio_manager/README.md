# Audio Manager

## Description
This component manages audio playback of WAV files using the I2S peripheral. It is designed to run in a dedicated FreeRTOS task, allowing for non-blocking audio playback control.

## Features
-   Plays `.wav` files from the filesystem (e.g., SD card).
-   Supports basic playback controls: Play, Pause, Resume, and Stop.
-   Provides volume control (Up/Down).
-   Exposes functions to get the current playback state, total duration, and progress.
-   Handles I2S driver initialization and teardown automatically.
-   Parses basic WAV headers to configure I2S parameters like sample rate and bit depth.

## How to Use

1.  **Initialize the Manager:**
    Call this function once at application startup. It sets up initial state but does not configure hardware yet.
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
    uint8_t volume_percent = audio_manager_get_volume();

    // Update UI based on the values...
    ```

## Dependencies
-   Requires an I2S-compatible audio codec/amplifier (e.g., MAX98357A) connected to the I2S pins defined in `config.h`.
-   Depends on a mounted filesystem (like the `sd_card_manager`) to access audio files.

## Limitations
-   Currently supports only WAV file format.
-   The WAV parser is basic and may not support all sub-chunks or variations of the format. It expects standard `fmt` and `data` chunks.