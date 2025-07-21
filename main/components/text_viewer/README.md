# Text Viewer Component

## Description
A simple, reusable LVGL component for displaying read-only text content in a full-screen view. It is ideal for showing the contents of a text file, a transcription result, or help information.

## Features
-   **Full-Screen Display:** Creates a view with a title and a large, scrollable text area.
-   **Automatic Memory Management:** Takes ownership of the dynamically allocated text buffer (`char*`) and automatically frees it when the component is destroyed, preventing memory leaks.
-   **Exit Callback:** Uses a callback function to notify the parent view when the user wants to exit (by pressing the "Cancel" button). The callback now supports a `void* user_data` parameter for passing context.
-   **Focused Input:** Registers its own button handlers, ensuring only the "Cancel" button is active to provide a simple, predictable user experience.

## How to Use

1.  **Get your text content:**
    First, you need your text in a dynamically allocated buffer (e.g., from `malloc`, `strdup`, or `sd_manager_read_file`).
    ```cpp
    #include "controllers/sd_card_manager/sd_card_manager.h"

    char* file_content = NULL;
    size_t file_size = 0;
    if (sd_manager_read_file("/sdcard/notes/my_note.txt", &file_content, &file_size)) {
        // Content is now in file_content, ready to be displayed.
    }
    ```

2.  **Create the Component:**
    Instantiate the viewer, passing the parent object, a title, the content buffer, an exit callback, and a context pointer.
    ```cpp
    #include "components/text_viewer/text_viewer.h"

    // Forward declaration of the exit handler
    void on_viewer_exit(void* user_data);

    // In your view logic:
    if (file_content) {
        text_viewer_create(
            lv_screen_active(),     // Parent object
            "my_note.txt",          // Title
            file_content,           // The content to display
            on_viewer_exit,         // Function to call on exit
            this                    // Context pointer for the callback
        );
        // CRITICAL: Do NOT free(file_content) here! The component now owns it.
    }
    ```

3.  **Implement the Exit Callback:**
    This function defines what happens when the user closes the viewer. It now receives the `user_data` pointer.
    ```cpp
    void on_viewer_exit(void* user_data) {
        // The text_viewer is a child of the screen. Cleaning the screen
        // will automatically destroy it and trigger its cleanup logic.
        lv_obj_clean(lv_screen_active());
        
        // Now, load the view you want to return to.
        view_manager_load_view(VIEW_ID_VOICE_NOTE);
    }
    ```