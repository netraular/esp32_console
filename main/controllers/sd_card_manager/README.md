# SD Card Manager

## Description
This component manages access to a Micro SD card through a dedicated SPI bus. It handles the hardware initialization, mounting of the FAT filesystem, and provides a comprehensive interface for file and directory operations.

## Features
-   **Dedicated SPI Bus:** Initializes and uses its own dedicated SPI bus (`SPI3_HOST`) to avoid conflicts with other peripherals like the screen.
-   **Filesystem Mounting:** Mounts the SD card using the FAT filesystem via ESP-IDF's VFS (Virtual File System). The card is accessible at the mount point `/sdcard`.
-   **File Iteration:** Provides a utility function (`sd_manager_list_files`) to iterate through files and directories in a given path, using a callback for each entry.
-   **Status Check:** Includes functions to check if the card is currently mounted.
-   **File Management:** Offers a full suite of file management functions:
    -   Create files and directories (`sd_manager_create_file`, `sd_manager_create_directory`).
    -   Delete files and empty directories (`sd_manager_delete_item`).
    -   Rename or move items (`sd_manager_rename_item`).
    -   Read the entire content of a file into a buffer (`sd_manager_read_file`).

## How to Use

1.  **Initialize the Manager:**
    Call this function at application startup. It handles the initialization of the SPI bus and mounts the filesystem.
    ```cpp
    #include "controllers/sd_card_manager/sd_card_manager.h"
    
    if (sd_manager_init()) {
        ESP_LOGI(TAG, "SD Card hardware initialized.");
    } else {
        ESP_LOGE(TAG, "Failed to initialize SD Card hardware.");
    }
    ```
    *Note: The filesystem is now mounted on demand (e.g., by `sd_manager_mount()` or `sd_manager_check_ready()`).*

2.  **Check and Mount:**
    Before accessing files, ensure the card is ready.
    ```cpp
    if (sd_manager_check_ready()) { // This will attempt to mount if not already mounted
        // Safe to access files at "/sdcard"
    }
    ```

3.  **Perform File Operations:**
    Use the new functions to manage your filesystem.
    ```cpp
    const char* base_path = sd_manager_get_mount_point();

    // Create a directory
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "%s/my_new_folder", base_path);
    sd_manager_create_directory(dir_path);

    // Create a file
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/my_file.txt", base_path);
    sd_manager_create_file(file_path);

    // Rename/Move the file
    char new_file_path[256];
    snprintf(new_file_path, sizeof(new_file_path), "%s/my_new_folder/renamed.txt", base_path);
    sd_manager_rename_item(file_path, new_file_path);

    // Delete the file
    sd_manager_delete_item(new_file_path);

    // Delete the directory (must be empty)
    sd_manager_delete_item(dir_path);
    ```

4.  **List Files with a Callback:**
    The `sd_manager_list_files` function is ideal for populating a file explorer UI.
    ```cpp
    void my_file_iterator(const char *name, bool is_dir, void *user_data) {
        printf("%s: %s\n", is_dir ? "DIR" : "FILE", name);
    }
    
    if (sd_manager_is_mounted()) {
        sd_manager_list_files(sd_manager_get_mount_point(), my_file_iterator, NULL);
    }
    ```

5.  **Deinitialize:**
    To safely unmount the card and free the SPI bus resources.
    ```cpp
    sd_manager_deinit();
    ```

## Dependencies
-   Requires an SD card connected to the dedicated SPI pins defined in `config.h`.