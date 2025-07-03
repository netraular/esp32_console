# File Explorer Component

## Description
A reusable LVGL component that provides a fully functional file and directory browser. It is designed to be embedded within any view or screen of an application.

## Features
-   **Filesystem Navigation:** Allows browsing directories, including navigating to parent directories.
-   **Callback-driven:** Interacts with the parent view through callbacks for file selection, actions (like "create file"), and exiting.
-   **Dynamic Content:** Reads and displays the contents of a given path from any mounted VFS (like an SD card).
-   **Sorting:** Automatically sorts entries, displaying directories first, then files, both in alphabetical order.
-   **Input Handling:** Manages its own button input for navigation (Up/Down/Select/Back) and deactivates its handlers when not in focus.
-   **Error Handling:** Displays a message if the underlying storage (e.g., SD card) becomes unreadable.
-   **Extensible Actions:** Can display "action" buttons (e.g., Create File, Create Folder) at the top of the list, firing a callback when selected.

## How to Use

1.  **Create the Component:**
    Instantiate the explorer within a container object in your view. Provide callbacks to handle user actions.
    ```cpp
    #include "components/file_explorer/file_explorer.h"

    // In your view's create function:
    lv_obj_t* explorer_container = lv_obj_create(parent);
    // ... set size and position for explorer_container ...

    file_explorer_create(
        explorer_container,                 // Parent object
        "/sdcard",                          // Initial path to show
        my_on_file_selected_callback,       // Func to call when a file is chosen
        my_on_action_callback,              // Func to call for actions like "Create"
        my_on_exit_callback                 // Func to call when user exits the explorer
    );
    ```

2.  **Implement Callbacks:**
    Define the functions that will react to the explorer's events.
    ```cpp
    void my_on_file_selected_callback(const char* file_path) {
        // User selected a file, e.g., show its content or play it.
        printf("File selected: %s\n", file_path);
    }

    void my_on_action_callback(file_item_type_t action, const char* current_path) {
        // User wants to create a new file or folder in current_path
    }

    void my_on_exit_callback(void) {
        // User wants to close the explorer.
        // Your view should now destroy the explorer and restore its own UI.
    }
    ```

3.  **Manage Input Focus (Optional):**
    If your view has other interactive elements, you can activate/deactivate the explorer's input handlers.
    ```cpp
    // Give control to the explorer
    file_explorer_set_input_active(true);

    // Take control back from the explorer
    file_explorer_set_input_active(false);
    ```

4.  **Refresh Content:**
    If you perform a file operation (create, delete, rename) outside the explorer, you can tell it to reload its list.
    ```cpp
    file_explorer_refresh();
    ```