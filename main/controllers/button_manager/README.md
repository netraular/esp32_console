# Button Manager

## Description
This component manages the system's physical buttons using the `espressif/button` library. It provides an abstraction layer to register single-click events and handle two-level callbacks: default and view-specific.

## Usage
1.  **Initialize the Button Manager:**
    Call this function once at application startup.
    ```cpp
    #include "controllers/button_manager/button_manager.h"
    
    // ...
    
    button_manager_init();
    ```
    This sets up the pins and assigns default handlers that simply print a log message.

2.  **Register Specific Handlers (Optional):**
    If a particular screen or application state needs a button to do something different, a `view_handler` can be registered.
    ```cpp
    void my_custom_ok_action() {
        // Do something special
    }
    
    button_manager_register_view_handler(BUTTON_OK, my_custom_ok_action);
    ```

3.  **Restore Default Handlers:**
    When exiting that special screen or state, the default behaviors should be restored.
    ```cpp
    button_manager_unregister_view_handlers();
    ```