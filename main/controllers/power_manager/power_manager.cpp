#include "power_manager.h"
#include "config.h" 
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time.h"

// Include managers needed for wake-up action
#include "controllers/button_manager/button_manager.h"
#include "controllers/notification_manager/notification_manager.h"
#include <sys/stat.h>

static const char* TAG = "POWER_MGR";

void power_manager_enter_light_sleep(void) {
    ESP_LOGI(TAG, "Preparing to enter light sleep mode...");

    // Give button manager time to process any final events and pause
    button_manager_pause_for_wake_up(500);

    // --- Wake-up Source 1: GPIO (On/Off Button) ---
    // Step 1: Configure the specific pin and trigger level. This MUST be done every time.
    ESP_ERROR_CHECK(gpio_wakeup_enable(BUTTON_ON_OFF_PIN, GPIO_INTR_LOW_LEVEL));
    // Step 2: Enable the GPIO peripheral as a general wakeup source.
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
    
    // --- Wake-up Source 2: Timer (for Notifications) ---
    time_t now = time(NULL);
    // Only set a timer if time is synchronized (a simple check for a reasonable timestamp)
    if (now > 1672531200) { // A timestamp in early 2023
        time_t next_notif_ts = NotificationManager::get_next_notification_timestamp();
        if (next_notif_ts > now) {
            uint64_t sleep_duration_s = next_notif_ts - now;
            // Add a small buffer to ensure we wake up just after the notification time
            uint64_t sleep_duration_us = (sleep_duration_s + 1) * 1000000;
            ESP_LOGI(TAG, "Setting light sleep timer wakeup in %llu seconds for next notification.", sleep_duration_s);
            esp_sleep_enable_timer_wakeup(sleep_duration_us);
        }
    } else {
        ESP_LOGI(TAG, "System time not synced, skipping timer wakeup setup.");
    }

    ESP_LOGI(TAG, "Entering light sleep. Wake-up source(s) configured. System will now pause.");
    vTaskDelay(pdMS_TO_TICKS(30)); // Allow logs to be flushed

    // Enter light sleep mode.
    esp_light_sleep_start();

    // --- Code resumes here after waking up ---
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "Woke up from light sleep! Cause: %d (Timer=4, GPIO=5)", cause);

    // Best practice: Disable all wakeup sources after waking up to ensure a clean state
    // for the next sleep cycle.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // Also explicitly disable the GPIO pin's wakeup function to return it to a normal state.
    gpio_wakeup_disable(BUTTON_ON_OFF_PIN);

    // If woken up by the timer, it was for a notification.
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        // Delegate the wake-up action to the NotificationManager to be handled in a safe context.
        NotificationManager::handle_wakeup_event();
    }
    
    ESP_LOGI(TAG, "Waiting for wake-up button to be released...");
    while (gpio_get_level(BUTTON_ON_OFF_PIN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGI(TAG, "Button released. Resuming normal operation.");
}

void power_manager_enter_deep_sleep(void) {
    ESP_LOGI(TAG, "Entering deep sleep mode. The device will turn off.");
    ESP_LOGI(TAG, "A hardware reset (RST button) will be required to wake up.");
    
    // Give time for logs to be written to the console.
    vTaskDelay(pdMS_TO_TICKS(100));

    // Enters deep sleep. This function does not return.
    esp_deep_sleep_start();
}