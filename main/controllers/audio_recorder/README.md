# Audio Recorder

## Description
This component manages audio recording from an I2S microphone and saves it as a standard WAV file. It operates in a dedicated FreeRTOS task to prevent blocking the main application.

## Features
-   Records audio from a configured I2S input.
-   Saves recordings in standard `.wav` format.
-   Applies a configurable digital gain to amplify the microphone signal.
-   Manages its own state (`IDLE`, `RECORDING`, `SAVING`, `CANCELLING`, `ERROR`).
-   Automatically generates and finalizes the WAV file header with correct size information.
-   Provides an API to start, stop, cancel, and query the status of the recording.

## How to Use

1.  **Initialize the Recorder:**
    Call this once at startup.
    ```cpp
    #include "controllers/audio_recorder/audio_recorder.h"
    
    audio_recorder_init();
    ```

2.  **Start Recording:**
    Provide a full file path. This will spawn a new task.
    ```cpp
    if (!audio_recorder_start("/sdcard/recordings/my_rec.wav")) {
        // Handle error (e.g., recorder was already busy)
    }
    ```

3.  **Stop or Cancel Recording:**
    These signal the task to finish writing the file or to discard it.
    ```cpp
    // To save the file:
    audio_recorder_stop();

    // To discard the file:
    audio_recorder_cancel();
    ```

4.  **Check Status:**
    This is essential for updating the UI.
    ```cpp
    audio_recorder_state_t state = audio_recorder_get_state();
    uint32_t duration_sec = audio_recorder_get_duration_s();
    
    if (state == RECORDER_STATE_ERROR) {
        // Handle error state, inform user.
    }
    ```

## Dependencies
-   Requires an I2S-compatible microphone (e.g., INMP441).
-   Depends on a mounted filesystem (like the `sd_card_manager`).
-   Requires I2S pins and recording parameters to be defined in `config.h`.