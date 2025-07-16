#include "power_manager.h"
#include "config.h" // Para BUTTON_ON_OFF_PIN
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "POWER_MGR";

void power_manager_enter_light_sleep(void) {
    ESP_LOGI(TAG, "Entering light sleep mode. Wake-up source: ON/OFF button.");

    // --- CORRECCIÓN: Procedimiento de 2 pasos para configurar el despertar por GPIO ---

    // 1. Configurar el pin específico y el nivel de activación para despertar.
    // Los botones son activos en bajo, por lo que usamos GPIO_INTR_LOW_LEVEL.
    esp_err_t err_config = gpio_wakeup_enable(BUTTON_ON_OFF_PIN, GPIO_INTR_LOW_LEVEL);
    if (err_config != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO wakeup pin: %s", esp_err_to_name(err_config));
        return;
    }

    // 2. Habilitar la fuente de despertar GPIO en general para el sistema de bajo consumo.
    esp_err_t err_enable = esp_sleep_enable_gpio_wakeup();
    if (err_enable != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable GPIO wakeup source: %s", esp_err_to_name(err_enable));
        // Deshacer la configuración del pin si la habilitación general falla
        gpio_wakeup_disable(BUTTON_ON_OFF_PIN);
        return;
    }

    // --- FIN DE LA CORRECCIÓN ---

    // Entrar en modo light sleep. La ejecución se pausará aquí hasta que ocurra un despertar.
    esp_err_t err_sleep = esp_light_sleep_start();

    // --- El código se reanuda aquí después de despertar ---

    if (err_sleep != ESP_OK) {
        // Este error puede ocurrir si otra fuente de despertar (que no configuramos) interrumpe el sueño.
        ESP_LOGW(TAG, "Light sleep was interrupted by an unexpected source: %s", esp_err_to_name(err_sleep));
    } else {
        // Registrar la causa del despertar para depuración.
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        ESP_LOGI(TAG, "Woke up from light sleep! Cause: %d (5=GPIO)", cause);
    }
    
    // Es una buena práctica deshabilitar la fuente de despertar GPIO después de usarla
    // para que no interfiera con el funcionamiento normal del botón.
    gpio_wakeup_disable(BUTTON_ON_OFF_PIN);
    
    // Añadimos un pequeño retardo para permitir que el usuario suelte el botón.
    // Esto previene que el 'button_manager' detecte un evento de "PRESS_UP" espurio
    // sin haber visto el "PRESS_DOWN" (porque el sistema estaba dormido).
    vTaskDelay(pdMS_TO_TICKS(500)); 
}