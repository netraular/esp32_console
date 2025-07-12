# Button Manager

## Description
This component manages the system's physical buttons using the `espressif/button` library. It provides a rich abstraction layer to handle various click events (tap, single, double, long press) and supports a two-level callback system: default and view-specific. It is designed to be UI-friendly by dispatching events through a queue processed in the LVGL timer context.

## Features
-   **Advanced Event Handling:** Distinguishes between `TAP` (fast press/release), `SINGLE_CLICK` (fires after a double-click timeout), `DOUBLE_CLICK`, and `LONG_PRESS`.
-   **Event Suppression:** Intelligently suppresses `TAP` and `SINGLE_CLICK` events if a `LONG_PRESS` occurs, preventing unwanted actions.
-   **Context-Aware Callbacks:** Handlers accept a `void* user_data` pointer, allowing components to pass their own state or instance (`this`) pointers, making them fully reusable and self-contained.
-   **Two-Level Handlers:** Supports `view_handlers` which take priority over `default_handlers`, allowing temporary overrides for specific screens.
-   **UI-Safe Dispatching:** By default, events are queued and processed in the LVGL thread, making it safe to call LVGL functions directly from button handlers. An "immediate" mode is available for low-latency, non-UI tasks.

## How to Use

1.  **Initialize the Manager:**
    Call this function once at application startup.
    ```cpp
    #include "controllers/button_manager/button_manager.h"
    
    button_manager_init();
    ```

2.  **Register a Handler:**
    To make a button perform an action, register a handler for a specific event. The `user_data` parameter is key for component reusability.
    ```cpp
    // In a component's method or a view's setup function:
    
    // Define the handler function with the correct signature
    void my_component_action(void* user_data) {
        // Cast user_data back to your component's type
        MyComponent* self = static_cast<MyComponent*>(user_data);
        self->do_something();
    }
    
    // Register the handler, passing 'this' or a context struct
    // The 'true' flag makes it a high-priority view_handler.
    button_manager_register_handler(
        BUTTON_OK, 
        BUTTON_EVENT_TAP, // The event to listen for
        my_component_action, 
        true, 
        this // Pass the component's instance pointer
    );
    ```
    If no context is needed, pass `nullptr`.
    ```cpp
    void my_global_action(void* user_data) {
        // No context needed here
    }

    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, my_global_action, true, nullptr);
    ```

3.  **Restore Default Handlers:**
    When a view is closed, it's crucial to unregister its specific handlers to avoid stale pointers and restore default behavior for the next view.
    ```cpp
    // Call this when your view is being destroyed
    button_manager_unregister_view_handlers();
    ```
    The `view_manager` calls this automatically when switching views.

## Dependencies
-   `espressif/button` component.
-   `lvgl/lvgl` for the queued dispatch mode timer.
-   Button pins and timings defined in `config.h`.