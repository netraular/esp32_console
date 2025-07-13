#include "standby_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "STANDBY_VIEW";

// Punteros para los widgets de la UI
static lv_obj_t *time_label;
static lv_obj_t *date_label;
static lv_timer_t *update_timer = NULL;

// Bandera para controlar el estado de la UI
static bool is_time_synced = false;

/**
 * @brief Tarea periódica para actualizar la hora y la fecha.
 */
static void update_time_task(lv_timer_t *timer) {
    if (wifi_manager_is_connected()) {
        // --- Transición de estado: Ocurre solo una vez cuando se conecta ---
        if (!is_time_synced) {
            is_time_synced = true;
            ESP_LOGI(TAG, "Time synchronized. Switching to clock display mode.");
            
            // Cambiar la fuente de la hora a una más grande
            lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
            
            // Re-centrar la etiqueta de la hora (la de la fecha ya está centrada)
            lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -20); // Mover un poco hacia arriba para hacer espacio
            lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 25);  // Mover hacia abajo
        }

        // --- Actualización continua (cada segundo) ---
        char time_str[16];
        char date_str[16];
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        // Formatear hora y fecha
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
        strftime(date_str, sizeof(date_str), "%d/%m/%Y", &timeinfo);
        
        // Actualizar las etiquetas
        lv_label_set_text(time_label, time_str);
        lv_label_set_text(date_label, date_str);

    } else {
        // --- Estado de conexión/desconexión ---
        if (is_time_synced) {
            // Si perdemos la conexión, volver al estado inicial
            is_time_synced = false;
            ESP_LOGI(TAG, "Connection lost. Reverting to connecting display mode.");
            // Volver a la fuente pequeña
            lv_obj_set_style_text_font(time_label, &lv_font_montserrat_24, 0);
            lv_obj_center(time_label); // Re-centrar la etiqueta de conexión
        }
        
        lv_label_set_text(time_label, "Connecting...");
        lv_label_set_text(date_label, ""); // Ocultar la fecha
    }
}

/**
 * @brief Manejador para el botón CANCEL. Limpia los recursos y carga la vista del menú.
 */
static void handle_cancel_press(void *user_data) {
    ESP_LOGI(TAG, "CANCEL pressed, cleaning up standby view and loading menu.");
    
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    
    view_manager_load_view(VIEW_ID_MENU);
}

/**
 * @brief Callback para el evento de borrado, asegura la limpieza del timer.
 */
static void cleanup_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Standby view is being deleted, ensuring timer is removed.");
        if (update_timer) {
            lv_timer_del(update_timer);
            update_timer = NULL;
        }
        is_time_synced = false; // Resetear estado para la próxima vez que se cargue la vista
    }
}

void standby_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Standby View");
    
    // Resetear el estado al crear la vista
    is_time_synced = false;

    // --- Configuración de la etiqueta de la hora (inicialmente pequeña) ---
    time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(time_label, "Loading...");
    lv_obj_center(time_label);

    // --- Configuración de la etiqueta de la fecha (siempre pequeña) ---
    date_label = lv_label_create(parent);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(date_label, ""); // Inicialmente vacía
    
    // --- CORRECCIÓN: Alinear la fecha al centro con un desplazamiento Y ---
    // En el estado de conexión, estará debajo de la etiqueta "Connecting...".
    // Su posición final se ajustará cuando se sincronice la hora.
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 25);


    // Crear un timer para actualizar la UI
    update_timer = lv_timer_create(update_time_task, 1000, NULL);

    // Registrar manejadores de botones
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, NULL, true, nullptr);

    // Añadir un evento de limpieza para cuando la vista sea destruida
    lv_obj_add_event_cb(parent, cleanup_event_cb, LV_EVENT_DELETE, NULL);
}