/**
 * @file main.cpp
 * @brief Aplicación que muestra un contador en la pantalla.
 */

// --- HEADERS NECESARIOS ---
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h> // Para snprintf

// extern "C" es necesario para incluir headers de C (como LVGL) en un archivo C++
extern "C" {
#include "lvgl.h"
}

// Incluimos solo el controlador de la pantalla
#include "controllers/screen_manager/screen_manager.h"

static const char *TAG = "CONTADOR_MAIN";

// Puntero global a la etiqueta del contador para poder actualizarla
static lv_obj_t* counter_label;

/**
 * @brief Crea la etiqueta del contador en la pantalla.
 */
void create_counter_ui(lv_obj_t* parent) {
    counter_label = lv_label_create(parent);
    lv_label_set_text(counter_label, "0");
    lv_obj_set_style_text_font(counter_label, &lv_font_montserrat_48, 0);
    lv_obj_center(counter_label);
    ESP_LOGI(TAG, "UI del contador creada.");
}


// --- PUNTO DE ENTRADA DE LA APLICACIÓN ---
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "--- Iniciando aplicación 'Contador' ---");

    // --- 1. Inicializar la pantalla y LVGL ---
    screen_t* screen = screen_init();
    if (!screen) {
        ESP_LOGE(TAG, "Fallo al inicializar la pantalla. Deteniendo.");
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    ESP_LOGI(TAG, "Controlador de pantalla inicializado.");

    // --- 2. Crear la interfaz de usuario ---
    create_counter_ui(lv_screen_active());

    // --- 3. Bucle principal con la lógica del contador ---
    ESP_LOGI(TAG, "Entrando en el bucle principal.");
    
    static int counter = 0;
    char buf[16];

    while (true) {
        // Actualizar el texto del contador
        snprintf(buf, sizeof(buf), "%d", counter);
        lv_label_set_text(counter_label, buf);
        
        // Incrementar el contador
        counter++;

        // Procesar tareas de LVGL (muy importante para redibujar)
        // Usamos un delay más corto aquí para que la UI sea más fluida
        // si se añaden más elementos en el futuro.
        lv_timer_handler(); 
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Actualizamos el contador solo una vez por segundo
        static uint32_t last_update = 0;
        if (esp_log_timestamp() - last_update > 1000) {
            last_update = esp_log_timestamp();
            // La lógica del contador ya no necesita estar aquí,
            // podemos simplificar el bucle.
        }
    }
}