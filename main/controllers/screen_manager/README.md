# Screen Manager

## Description
This component handles all low-level initialization for the device's display. It configures the SPI bus, the display controller (ST7789), and the LVGL graphics library, acting as the fundamental bridge between the UI software and the physical hardware.

## Features
- **SPI Bus Initialization:** Configures and starts the dedicated `SPI2_HOST` for communication with the display.
- **Display Controller:** Initializes the ST7789 LCD panel driver, including pin configuration (CS, DC, RST) and communication parameters.
- **Backlight Control:** Manages the backlight pin (`TFT_BL`) to turn it on during initialization.
- **LVGL Integration:**
    - Initializes the LVGL library.
    - Creates and configures a periodic `esp_timer` to provide the necessary "tick" for LVGL's operation (`lv_tick_inc`).
    - Allocates two drawing buffers in DMA-capable RAM, enabling efficient partial-mode rendering.
    - Provides the `lvgl_flush_cb` callback that transfers the rendered data from LVGL to the physical display via SPI.

## How to Use

1.  **Initialize the Manager:**
    Call this function once at application startup. It returns a pointer to a `screen_t` structure containing the necessary handles, or `nullptr` if it fails.
    ```cpp
    #include "controllers/screen_manager/screen_manager.h"

    // In your app_main function
    screen_t* screen = screen_init();
    if (!screen) {
        ESP_LOGE(TAG, "Failed to initialize screen, halting.");
        return;
    }
    ```

2.  **Handle the LVGL Loop:**
    After initialization, the application's main loop must call `lv_timer_handler()` periodically for LVGL to process its tasks, such as animations and events.
    ```cpp
    while (true) {
        // Handle LVGL tasks
        lv_timer_handler();
        // Delay to yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ```

## Dependencies
-   **ESP-IDF:** `esp_lcd` and `driver/spi_master` components.
-   **Third-party Libraries:** `lvgl/lvgl`.
-   **Local Configuration:** Requires that display pins and parameters (e.g., `SPI_SCLK_PIN`, `TFT_CS`, `SCREEN_WIDTH`) are correctly defined in `config.h`.