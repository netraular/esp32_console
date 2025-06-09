# SD Card Manager

## Description
This component manages access to a Micro SD card through a dedicated SPI bus. It handles the hardware initialization, mounting of the FAT filesystem, and provides an interface for file operations.

## Features
-   **Dedicated SPI Bus:** Initializes and uses its own dedicated SPI bus (`SPI3_HOST`) to avoid conflicts with other peripherals like the screen.
-   **Filesystem Mounting:** Mounts the SD card using the FAT filesystem via ESP-IDF's VFS (Virtual File System). The card is accessible at the mount point `/sdcard`.
-   **File Iteration:** Provides a utility function (`sd_manager_list_files`) to iterate through files and directories in a given path, using a callback for each entry.
-   **Status Check:** Includes functions to check if the card is currently mounted.

## How to Use

1.  **Initialize the Manager:**
    Call this function at application startup. It handles the initialization of the SPI bus and mounts the filesystem. It is independent of other managers like `screen_manager`.
    ```cpp
    #include "controllers/sd_card_manager/sd_card_manager.h"
    
    if (sd_manager_init()) {
        ESP_LOGI(TAG, "SD Card mounted successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to mount SD Card.");
    }
    ```

2.  **Check if Mounted:**
    Before accessing files, you can check if the card is ready.
    ```cpp
    if (sd_manager_is_mounted()) {
        // Safe to access files at "/sdcard"
    }
    ```

3.  **List Files with a Callback:**
    The `sd_manager_list_files` function is ideal for populating a file explorer UI. You provide a callback function that will be executed for each file and directory found.
    ```cpp
    // Define a callback function
    void my_file_iterator(const char *name, bool is_dir, void *user_data) {
        const char *type = is_dir ? "Directory" : "File";
        printf("%s: %s\n", type, name);
    }

    // Call the list function
    if (sd_manager_is_mounted()) {
        const char *path = sd_manager_get_mount_point(); // Get "/sdcard"
        sd_manager_list_files(path, my_file_iterator, NULL);
    }
    ```

4.  **Deinitialize:**
    To safely unmount the card and free the SPI bus resources.
    ```cpp
    sd_manager_deinit();
    ```

## Dependencies
-   Requires an SD card connected to the dedicated SPI pins defined in `config.h` (`SD_SPI_SCLK_PIN`, `SD_SPI_MOSI_PIN`, `SD_SPI_MISO_PIN`, `SD_CS_PIN`).