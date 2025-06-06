#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"

// Incluir los controladores principales
#include "controllers/screen_manager/screen_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"

// Incluir el nuevo gestor de vistas
#include "views/view_manager.h"

static const char *TAG = "main";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Iniciando aplicación");

    // 1. Inicialización de la pantalla (hardware y LVGL)
    screen_t* screen = screen_init();
    if (!screen) {
        ESP_LOGE(TAG, "Fallo al inicializar la pantalla, deteniendo.");
        return;
    }

    // 1.1. Inicialización de la tarjeta SD (usa el bus SPI ya inicializado por la pantalla)
    if (sd_manager_init()) {
        ESP_LOGI(TAG, "SD Card manager inicializado correctamente.");
    } else {
        ESP_LOGE(TAG, "Fallo al inicializar SD Card manager.");
    }

    // 2. Inicialización de los botones
    button_manager_init();
    ESP_LOGI(TAG, "Button manager inicializado.");

    // 3. Inicializar el gestor de vistas, que se encargará de crear la UI.
    view_manager_init();
    ESP_LOGI(TAG, "View manager inicializado y vista principal cargada.");

    // 4. Bucle principal para manejar LVGL
    ESP_LOGI(TAG, "Entrando en bucle principal");
    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}