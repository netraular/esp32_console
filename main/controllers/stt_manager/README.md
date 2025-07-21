# Speech-to-Text (STT) Manager

## Description
This component provides an interface to transcribe audio files using a remote STT service. It is currently configured to use the Groq API with the `whisper-large-v3-turbo` model.

## Features
-   **Asynchronous Transcription:** Performs API requests in a dedicated FreeRTOS task to avoid blocking the main application or UI.
-   **Groq API Integration:** Handles the `multipart/form-data` request format required by the Groq audio transcription endpoint.
-   **Callback-driven:** Notifies the calling module of the transcription result (success or failure) via a callback function. The callback now supports a `void* user_data` parameter for passing context.
-   **Secure Communication:** Uses the embedded CA certificate for HTTPS and retrieves the API key from `secret.h` to keep it out of source control.
-   **Efficient Streaming:** Sends the audio file in chunks directly from the filesystem to the server, avoiding the need to load the entire file into RAM.
-   **Prerequisite Checks:** Automatically waits for a WiFi connection and time synchronization before starting the request.

## How to Use

1.  **Configure API Key and Certificate:**
    - Add your Groq API key to `secret.h`:
      ```c
      #define GROQ_API_KEY "Your_Groq_API_Key"
      ```
    - Ensure the Groq CA certificate (`groq_api_ca.pem`) is embedded via `CMakeLists.txt`.

2.  **Initialize the Manager:**
    Call this once at application startup.
    ```cpp
    #include "controllers/stt_manager/stt_manager.h"
    
    stt_manager_init();
    ```

3.  **Start a Transcription:**
    Provide a file path, a callback function, and a context pointer to handle the result.
    ```cpp
    // Define the callback function
    void on_transcription_complete(bool success, char* result, void* user_data) {
        // You can use the context if needed:
        // MyView* self = static_cast<MyView*>(user_data);

        if (success) {
            printf("Transcription: %s\n", result);
            // The receiver of this callback is responsible for freeing the 'result' buffer.
            // For example, by passing it to a text_viewer component which handles the free.
        } else {
            printf("Transcription failed: %s\n", result);
            free(result); // Free the error message buffer
        }
    }

    // Call the manager to start the process
    if (!stt_manager_transcribe("/sdcard/notes/my_note.wav", on_transcription_complete, this)) {
        // Handle error, e.g., task creation failed
    }
    ```

## Dependencies
-   **ESP-IDF:** `esp_http_client`, `cJSON`, `esp_event`.
-   **Local:** `secret.h` for the API key, `wifi_manager` for network status, `sd_card_manager` to read files.
-   Requires an active WiFi connection.