#include "power_manager.h"
#include "config.h" 
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time.h"
#include <sys/stat.h>

// Include managers needed for direct wake-up actions
#include "controllers/button_manager/button_manager.h"
#include "controllers/notification_manager/notification_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/screen_manager/screen_manager.h"

/**
 * @brief DESIGN NOTE: Light Sleep Notification Handling
 *
 * This is the INTENTIONAL behavior for timer-based (notification) wakeups:
 * - The device wakes.
 * - It plays the notification sound ONLY.
 * - The screen remains OFF.
 * - It immediately returns to sleep.
 *
 * This is critical for battery saving. The user can view missed notifications
 * in the history view after manually waking the device.
 *
 * *** DO NOT attempt to show UI/popups from this power manager. ***
 */
// --- END NEW COMMENT ---

static const char* TAG = "POWER_MGR";
static const char* NOTIFICATION_SOUND_PATH = "/sdcard/sounds/notification.wav";

void power_manager_enter_light_sleep(void) {
sleep_entry_point:
    ESP_LOGI(TAG, "Preparing to enter light sleep mode...");

    // Pause button processing briefly to avoid spurious events during sleep transition
    button_manager_pause_for_wake_up(50); 

    // --- Wake-up Source 1: GPIO (On/Off Button) ---
    ESP_ERROR_CHECK(gpio_wakeup_enable(BUTTON_ON_OFF_PIN, GPIO_INTR_LOW_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
    
    // --- Wake-up Source 2: Timer (for Notifications) ---
    time_t now = time(NULL);
    // Only set a timer if time is synchronized
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

    ESP_LOGI(TAG, "Turning backlight OFF for sleep.");
    screen_set_backlight(false);
    
    ESP_LOGI(TAG, "Entering light sleep. Wake-up source(s) configured. System will now pause.");
    vTaskDelay(pdMS_TO_TICKS(30)); // Allow logs to be flushed

    esp_light_sleep_start();

    // --- Code resumes here after waking up ---
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "Woke up from light sleep! Cause: %d (Timer=4, GPIO=5)", cause);

    // Best practice: Disable all wakeup sources after waking up
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    gpio_wakeup_disable(BUTTON_ON_OFF_PIN);

    // --- Handle Wake-up Cause ---
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        // Woken by notification timer. Play sound and go back to sleep.
        ESP_LOGI(TAG, "Wakeup by timer. Playing notification sound and returning to sleep.");
        
        if (sd_manager_check_ready()) {
            struct stat st;
            if (stat(NOTIFICATION_SOUND_PATH, &st) == 0) {
                audio_manager_play(NOTIFICATION_SOUND_PATH);
                // Wait for the sound to finish playing.
                while (audio_manager_get_state() != AUDIO_STATE_STOPPED) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                ESP_LOGI(TAG, "Sound finished.");
            } else {
                ESP_LOGW(TAG, "Notification sound file not found at %s", NOTIFICATION_SOUND_PATH);
            }
        } else {
            ESP_LOGW(TAG, "SD card not ready, cannot play notification sound.");
        }
        
        goto sleep_entry_point; // Re-enter sleep cycle, screen remains OFF
    }

    // --- Woken up by GPIO (user) or other source. Resume normal operation. ---
    ESP_LOGI(TAG, "Woken up by user, turning backlight ON.");
    screen_set_backlight(true);

    ESP_LOGI(TAG, "Waiting for wake-up button to be released...");
    while (gpio_get_level(BUTTON_ON_OFF_PIN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGI(TAG, "Button released. Resuming normal operation.");
}

void power_manager_enter_deep_sleep(void) {
    ESP_LOGI(TAG, "Entering deep sleep mode. The device will turn off.");
    ESP_LOGI(TAG, "A hardware reset (RST button) will be required to wake up.");
    
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_deep_sleep_start();
}