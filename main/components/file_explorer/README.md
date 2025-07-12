# File Explorer Component

## Description
A reusable LVGL component that provides a fully functional file and directory browser. It is designed to be embedded within any view and interacts with the parent view through a set of callbacks.

## Features
-   **Filesystem Navigation:** Navigate directory structures, including moving to parent directories.
-   **Callback-driven:** Interacts with the parent view via callbacks for file selection, long-press actions, creation actions, and exiting.
-   **Dynamic Content:** Reads and displays the contents of a given path from a mounted VFS (e.g., SD card).
-   **Sorting:** Automatically sorts entries, displaying directories first, then files, both in case-insensitive alphabetical order.
-   **Input Handling:** Manages its own button input for navigation (Up/Down/Select/Back) and can be activated or deactivated.
-   **Error Handling:** Displays a message if the underlying storage (e.g., SD card) becomes unreadable.
-   **Extensible Actions:** Can display "action" items (e.g., Create File) at the top of the list, firing a callback when selected.

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
        my_on_file_selected_callback,       // Func for TAP on a file
        my_on_file_long_press_callback,     // Func for LONG PRESS on a file (can be NULL)
        my_on_action_callback,              // Func for actions like "Create" (can be NULL)
        my_on_exit_callback                 // Func for when user exits the explorer
    );
    ```

2.  **Implement Callbacks:**
    Define the functions that will react to the explorer's events.
    ```cpp
    void my_on_file_selected_callback(const char* file_path) {
        printf("File selected: %s\n", file_path);
        // User selected a file, e.g., open it in a player or viewer.
    }
    
    void my_on_file_long_press_callback(const char* file_path) {
        printf("File long-pressed: %s\n", file_path);
        // User long-pressed a file, e.g., show a delete confirmation popup.
    }

    void my_on_action_callback(file_item_type_t action, const char* current_path) {
        // User wants to create a new file or folder in current_path.
    }

    void my_on_exit_callback(void) {
        // User wants to close the explorer.
        // Your view should now destroy the explorer and restore its own UI.
    }
    ```

3.  **Manage Input Focus (Optional):**
    If your view has other interactive elements (like a popup), you can activate/deactivate the explorer's input handlers.
    ```cpp
    // Take control back from the explorer
    file_explorer_set_input_active(false);
    // ... show popup and handle its input ...
    
    // Give control back to the explorer
    file_explorer_set_input_active(true);
    ```

4.  **Refresh Content:**
    If you perform a file operation (create, delete, rename), tell the explorer to reload its list.
    ```cpp
    file_explorer_refresh();
    ```