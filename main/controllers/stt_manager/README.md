# Speech-to-Text (STT) Manager

## Description
This component provides an interface to transcribe audio files using a remote STT service. It is currently configured to use the Groq API with the `whisper-large-v3-turbo` model.

## Features
-   **Asynchronous Transcription:** Performs API requests in a dedicated FreeRTOS task to avoid blocking the main application or UI.
-   **Groq API Integration:** Handles the specific `multipart/form-data` request format required by the Groq audio transcription endpoint.
-   **Callback-driven:** Notifies the calling module of the transcription result (success or failure) via a callback function.
-   **Secure Credentials:** Retrieves the API key from `secret.h`, keeping it out of source control.
-   **Efficient Streaming:** Sends the audio file in chunks directly from the filesystem to the server, avoiding the need to load the entire file into RAM.

## How to Use

1.  **Configure API Key:**
    Add your Groq API key to `secret.h`:
    ```c
    #define GROQ_API_KEY "Your_Groq_API_Key"
    ```

2.  **Initialize the Manager:**
    Call this once at application startup.
    ```cpp
    #include "controllers/stt_manager/stt_manager.h"
    
    stt_manager_init();
    ```

3.  **Start a Transcription:**
    Provide a file path and a callback function to handle the result.
    ```cpp
    // Define the callback function
    void on_transcription_complete(bool success, char* result) {
        if (success) {
            printf("Transcription: %s\n", result);
            // The receiver of this callback is responsible for freeing the 'result' buffer
            // For example, by passing it to a text_viewer component.
        } else {
            printf("Transcription failed: %s\n", result);
            free(result); // Free the error message buffer
        }
    }

    // Call the manager to start the process
    if (!stt_manager_transcribe("/sdcard/notes/my_note.wav", on_transcription_complete)) {
        // Handle error, e.g., task creation failed
    }
    ```

## Dependencies
-   **ESP-IDF:** `esp_http_client` and `cJSON`.
-   **Local:** `secret.h` for the API key, `sd_card_manager` to read files.
-   Requires an active WiFi connection.