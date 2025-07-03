# Text Viewer Component

## Description
A simple, reusable LVGL component for displaying read-only text content, such as the contents of a file. It is designed to take over the full screen and provide a clean, focused reading experience.

## Features
-   **Full-Screen Display:** Creates a view with a title and a scrollable text area.
-   **Memory Management:** Takes ownership of the dynamically allocated text buffer (`char*`) and automatically frees it when the component is destroyed, preventing memory leaks.
-   **Exit Callback:** Uses a callback function to notify the parent view when the user wants to exit (by pressing the "Cancel" button).
-   **Focused Input:** Registers its own button handlers, ensuring only the "Cancel" button is active, preventing unintended actions.

## How to Use

1.  **Read your data:**
    First, you need to have your text content in a dynamically allocated buffer (e.g., from `malloc` or `sd_manager_read_file`).
    ```cpp
    #include "controllers/sd_card_manager/sd_card_manager.h"

    char* file_content = NULL;
    size_t file_size = 0;
    if (sd_manager_read_file("/sdcard/my_file.txt", &file_content, &file_size)) {
        // Content is now in file_content, proceed to create the viewer.
    }
    ```

2.  **Create the Component:**
    Instantiate the viewer, passing the parent object (usually the screen), a title, the content buffer, and an exit callback.
    ```cpp
    #include "components/text_viewer/text_viewer.h"

    // Forward declaration of the exit handler
    void on_viewer_exit();

    // In your view logic:
    if (file_content) {
        text_viewer_create(
            lv_screen_active(),     // Parent object
            "my_file.txt",          // Title
            file_content,           // The content (viewer will free this later)
            on_viewer_exit          // Function to call on exit
        );
        // Do NOT free(file_content) here! The component will do it.
    }
    ```

3.  **Implement the Exit Callback:**
    This function is where you define what happens when the user closes the viewer. Typically, you would destroy the viewer and restore your previous view.
    ```cpp
    void on_viewer_exit() {
        // The text_viewer is usually a child of the screen, so cleaning the screen
        // will automatically destroy it and trigger its cleanup logic.
        // Or, if you have a pointer to the viewer object, call text_viewer_destroy().
        
        // Example: reload the file explorer
        show_my_file_explorer_view();
    }
    ```