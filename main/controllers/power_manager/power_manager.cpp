#include "power_manager.h"
#include "config.h" 
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "controllers/button_manager/button_manager.h"

static const char* TAG = "POWER_MGR";

void power_manager_enter_light_sleep(void) {
    ESP_LOGI(TAG, "Preparing to enter light sleep mode...");

    button_manager_pause_for_wake_up(500);

    // Configuración para despertar
    esp_err_t err_config = gpio_wakeup_enable(BUTTON_ON_OFF_PIN, GPIO_INTR_LOW_LEVEL);
    if (err_config != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO wakeup pin: %s", esp_err_to_name(err_config));
        return;
    }

    esp_err_t err_enable = esp_sleep_enable_gpio_wakeup();
    if (err_enable != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable GPIO wakeup source: %s", esp_err_to_name(err_enable));
        gpio_wakeup_disable(BUTTON_ON_OFF_PIN);
        return;
    }

    ESP_LOGI(TAG, "Entering light sleep. Wake-up source: ON/OFF button. System will now pause.");
    vTaskDelay(pdMS_TO_TICKS(30));

    // Entrar en modo light sleep.
    esp_err_t err_sleep = esp_light_sleep_start();

    // --- El código se reanuda aquí después de despertar ---
    gpio_wakeup_disable(BUTTON_ON_OFF_PIN);

    if (err_sleep != ESP_OK) {
        ESP_LOGW(TAG, "Light sleep was interrupted by an unexpected source: %s", esp_err_to_name(err_sleep));
    } else {
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        ESP_LOGI(TAG, "Woke up from light sleep! Cause: %d (5=GPIO)", cause);
    }
    
    ESP_LOGI(TAG, "Waiting for wake-up button to be released...");
    while (gpio_get_level(BUTTON_ON_OFF_PIN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGI(TAG, "Button released. Resuming normal operation.");
}

// --- NUEVA FUNCIÓN ---
void power_manager_enter_deep_sleep(void) {
    ESP_LOGI(TAG, "Entering deep sleep mode. The device will turn off.");
    ESP_LOGI(TAG, "A hardware reset (RST button) will be required to wake up.");
    
    // Dar tiempo a que los logs se escriban en la consola.
    vTaskDelay(pdMS_TO_TICKS(100));

    // Entra en deep sleep. Esta función no retorna.
    esp_deep_sleep_start();
}