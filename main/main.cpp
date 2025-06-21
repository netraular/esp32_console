#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"

#include "controllers/screen_manager/screen_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/audio_recorder/audio_recorder.h"
#include "views/view_manager.h"

static const char *TAG = "main";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Starting application");

    // Initialize display (hardware and LVGL)
    screen_t* screen = screen_init();
    if (!screen) {
        ESP_LOGE(TAG, "Failed to initialize screen, halting.");
        return;
    }

    // Initialize SD card hardware. The filesystem will be mounted on demand.
    if (sd_manager_init()) {
        ESP_LOGI(TAG, "SD Card manager hardware initialized successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to initialize SD Card manager hardware.");
    }

    // Initialize buttons
    button_manager_init();
    ESP_LOGI(TAG, "Button manager initialized.");

    // Initialize audio managers
    audio_manager_init();
    ESP_LOGI(TAG, "Audio manager (playback) initialized.");
    audio_recorder_init();
    ESP_LOGI(TAG, "Audio recorder initialized.");

    // Initialize the view manager, which creates the main UI.
    view_manager_init();
    ESP_LOGI(TAG, "View manager initialized and main view loaded.");

    // Main loop for LVGL handling
    ESP_LOGI(TAG, "Entering main loop");
    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}