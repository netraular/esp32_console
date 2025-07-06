# Data Manager

## Description
A centralized manager for handling persistent data storage using the ESP-IDF Non-Volatile Storage (NVS) library. It provides a simple, type-safe API to abstract away the underlying NVS implementation details.

## Features
-   **Key-Value Storage**: Saves and retrieves data using simple string keys.
-   **Type-Safe Accessors**: Provides functions for specific data types (e.g., `uint32_t`) to prevent errors.
-   **Abstraction**: Hides NVS handles, namespaces, and commit logic from the rest of the application.
-   **Error Handling**: Returns clear boolean success/failure and logs detailed errors.

## How to Use

1.  **Initialize the Manager:**
    Call this once at application startup, *after* initializing NVS flash.
    ```cpp
    #include "controllers/data_manager/data_manager.h"
    
    // In app_main() after nvs_flash_init()
    data_manager_init();
    ```

2.  **Save Data:**
    ```cpp
    uint32_t score = 100;
    if (data_manager_set_u32("high_score", score)) {
        // Success
    } else {
        // Handle error
    }
    ```

3.  **Retrieve Data:**
    This pattern is recommended: try to get the value, and if it fails (e.g., first boot), use a default.
    ```cpp
    uint32_t score;
    if (data_manager_get_u32("high_score", &score)) {
        // Value retrieved successfully
        printf("High score is %lu\n", score);
    } else {
        // Key not found or error, use a default value
        score = 0;
    }
    ```

## Dependencies
-   Requires `nvs_flash` to be initialized by the main application before calling `data_manager_init()`.