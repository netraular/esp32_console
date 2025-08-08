#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lvgl.h"
#include <cstring>

// Include all configuration headers
#include "config/board_config.h"
#include "config/app_config.h"
#include "config/secrets.h"

// Include all manager headers
#include "controllers/screen_manager/screen_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/littlefs_manager/littlefs_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/audio_recorder/audio_recorder.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/wifi_streamer/wifi_streamer.h"
#include "controllers/data_manager/data_manager.h"
#include "controllers/stt_manager/stt_manager.h"
#include "controllers/power_manager/power_manager.h"
#include "controllers/habit_data_manager/habit_data_manager.h"
#include "controllers/notification_manager/notification_manager.h"
#include "controllers/lvgl_vfs_driver/lvgl_fs_driver.h"
#include "controllers/weather_manager/weather_manager.h"
#include "controllers/pet_manager/pet_manager.h"
#include "views/view_manager.h"

static const char *TAG = "main";

// This function will write the embedded file to LittleFS if it doesn't exist.
static void provision_filesystem_data() {
    if (littlefs_manager_file_exists("welcome.txt")) {
        ESP_LOGI(TAG, "'welcome.txt' already exists, skipping provisioning.");
        return;
    }

    ESP_LOGI(TAG, "Provisioning 'welcome.txt' to LittleFS...");
    
    // Symbols created by the build system from the EMBED_FILES directive
    extern const uint8_t welcome_txt_start[] asm("_binary_welcome_txt_start");
    extern const uint8_t welcome_txt_end[]   asm("_binary_welcome_txt_end");
    const size_t welcome_txt_size = welcome_txt_end - welcome_txt_start;
    
    // Create a null-terminated string from the binary data
    char* content = (char*)malloc(welcome_txt_size + 1);
    if (!content) {
        ESP_LOGE(TAG, "Failed to allocate memory for provisioning file.");
        return;
    }
    memcpy(content, welcome_txt_start, welcome_txt_size);
    content[welcome_txt_size] = '\0';

    if (littlefs_manager_write_file("welcome.txt", content)) {
        ESP_LOGI(TAG, "Successfully wrote 'welcome.txt'");
    } else {
        ESP_LOGE(TAG, "Failed to write 'welcome.txt'");
    }
    free(content);
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Starting application");

    // Initialize NVS (Non-Volatile Storage), required for WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "Default event loop and netif created.");
    
    // Initialize Data Manager (depends on NVS)
    data_manager_init();
    ESP_LOGI(TAG, "Data manager initialized.");

    // Initialize display (hardware and LVGL)
    screen_t* screen = screen_init();
    if (!screen) {
        ESP_LOGE(TAG, "Failed to initialize screen, halting.");
        return;
    }

    // Initialize LittleFS on the 'storage' partition
    if (littlefs_manager_init("storage")) {
        ESP_LOGI(TAG, "LittleFS manager initialized.");
        provision_filesystem_data(); // Write files on first boot
    } else {
        ESP_LOGE(TAG, "Failed to initialize LittleFS manager.");
    }

    // Initialize the Habit Data Manager (depends on LittleFS)
    HabitDataManager::init();
    ESP_LOGI(TAG, "Habit data manager initialized.");


    // Initialize SD card hardware.
    if (sd_manager_init()) {
        ESP_LOGI(TAG, "SD Card manager hardware initialized.");
        // Proactively mount the SD card at startup.
        if (sd_manager_mount()) {
            ESP_LOGI(TAG, "SD Card mounted successfully during startup.");
        } else {
            ESP_LOGW(TAG, "Failed to mount SD Card during startup. Assets will not be available.");
        }
    } else {
        ESP_LOGE(TAG, "Failed to initialize SD Card manager hardware.");
    }

    // Use the custom VFS driver to bridge LVGL and the ESP-IDF VFS
    lvgl_fs_driver_init('S');

    // Initialize buttons
    button_manager_init();
    ESP_LOGI(TAG, "Button manager initialized.");

    // Initialize audio managers
    audio_manager_init();
    ESP_LOGI(TAG, "Audio manager (playback) initialized.");
    audio_recorder_init();
    ESP_LOGI(TAG, "Audio recorder initialized.");

    // Initialize network managers
    wifi_manager_init_sta();
    ESP_LOGI(TAG, "WiFi manager initialized.");
    wifi_streamer_init();
    ESP_LOGI(TAG, "WiFi streamer initialized.");
    
    // Initialize the Weather Manager (depends on WiFi)
    WeatherManager::init();
    ESP_LOGI(TAG, "Weather manager initialized.");

    // Initialize the Pet Manager (depends on time sync)
    PetManager::get_instance().init();

    // Initialize the Speech-to-Text manager
    stt_manager_init();
    ESP_LOGI(TAG, "STT manager initialized.");

    // Initialize Notification Manager
    NotificationManager::init();
    ESP_LOGI(TAG, "Notification manager initialized.");

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