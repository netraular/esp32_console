# Audio Player Component

## Description
A self-contained, full-screen LVGL component for playing WAV audio files. It provides a complete user interface including playback controls, progress display, and a real-time audio visualizer.

## Features
-   **Full UI:** Displays the song title, a progress slider, current/total time, and volume level.
-   **Integrated Visualizer:** Includes the `audio_visualizer` component to display a real-time spectrum of the audio.
-   **Playback Control:** Handles Play/Pause, Volume Up, and Volume Down actions using the `button_manager`.
-   **State Management:** Automatically starts playback on creation and manages its state through the `audio_manager`.
-   **Clean Exit:** Stops audio playback and unregisters its button handlers when the user exits, notifying the parent view via a callback.

## How to Use

1.  **Create the Component:**
    To open the player, create it on the active screen. Pass it the path to a WAV file and a callback function for when the player should close. The `user_data` pointer allows you to pass context (like `this`) to your callback.
    ```cpp
    #include "components/audio_player_component/audio_player_component.h"

    // Forward declare the exit handler
    void on_player_exit(void* user_data);

    // In your view's logic (e.g., when a file is selected in an explorer)
    const char* song_path = "/sdcard/music/track1.wav";
    
    // This will create the UI and start playback immediately
    audio_player_component_create(
        lv_screen_active(),
        song_path,
        on_player_exit,
        this // Pass 'this' or other context to the callback
    );
    ```

2.  **Implement the Exit Callback:**
    This function is responsible for cleaning up the player UI and returning to the previous view. It now receives the `user_data` pointer you passed during creation.
    ```cpp
    void on_player_exit(void* user_data) {
        // You can use user_data if needed, for example:
        // MyView* self = static_cast<MyView*>(user_data);
        
        // The player is a child of the screen. Cleaning the screen will
        // automatically call the player's internal cleanup logic.
        lv_obj_clean(lv_screen_active());
        
        // Now, load the previous view (e.g., the file explorer)
        view_manager_load_view(VIEW_ID_VOICE_NOTE);
    }
    ```

## Dependencies
-   `audio_manager` (for playback logic)
-   `button_manager` (for user input)
-   `audio_visualizer` (for the spectrum display)