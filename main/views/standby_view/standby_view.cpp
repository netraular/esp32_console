#include "standby_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/audio_manager/audio_manager.h" // <-- AÑADIR ESTE INCLUDE
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "STANDBY_VIEW";

// --- Variables para los widgets de la vista ---
static lv_obj_t *center_time_label;
static lv_obj_t *center_date_label;
static lv_obj_t *loading_label; // Etiqueta para el mensaje "Loading..."
static lv_timer_t *update_timer = NULL;
static bool is_time_synced = false;

// --- Tarea para actualizar el reloj central ---
static void update_center_clock_task(lv_timer_t *timer) {
    if (wifi_manager_is_connected()) {
        if (!is_time_synced) {
            is_time_synced = true;
            // Ocultar "Loading..." y mostrar el reloj
            lv_obj_add_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
        }

        // Actualizar hora y fecha
        char time_str[16];
        char date_str[64]; // Aumentar tamaño para nombres de día/mes largos
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
        strftime(date_str, sizeof(date_str), "%A, %d %B", &timeinfo);
        
        lv_label_set_text(center_time_label, time_str);
        lv_label_set_text(center_date_label, date_str);

    } else {
        if (is_time_synced) {
            is_time_synced = false;
            // Si se pierde la conexión, ocultar el reloj y mostrar "Loading..." de nuevo
            lv_obj_clear_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
        }
        // Se puede cambiar a "Connecting..." si se prefiere
        lv_label_set_text(loading_label, "Connecting...");
    }
}

// --- Manejador del botón para ir al menú ---
static void handle_cancel_press(void *user_data) {
    ESP_LOGI(TAG, "CANCEL pressed, cleaning up standby view and loading menu.");
    
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    
    view_manager_load_view(VIEW_ID_MENU);
}

// --- AÑADIDO: Manejadores para subir/bajar volumen ---
static void handle_volume_up(void *user_data) {
    audio_manager_volume_up();
}

static void handle_volume_down(void *user_data) {
    audio_manager_volume_down();
}
// --- FIN AÑADIDO ---

// --- Callback para limpieza de recursos ---
static void cleanup_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Standby view is being deleted, ensuring timer is removed.");
        if (update_timer) {
            lv_timer_del(update_timer);
            update_timer = NULL;
        }
        is_time_synced = false;
    }
}

// --- Creación de la vista ---
void standby_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Standby View");
    is_time_synced = false;

    // 1. Crear la barra de estado en la parte superior.
    status_bar_create(parent);

    // 2. Crear la etiqueta "Loading..."
    loading_label = lv_label_create(parent);
    lv_obj_set_style_text_font(loading_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(loading_label, "Loading...");
    lv_obj_center(loading_label);

    // 3. Crear el reloj y la fecha centrales (ocultos al inicio).
    center_time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(center_time_label, &lv_font_montserrat_48, 0);
    lv_obj_align(center_time_label, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);

    center_date_label = lv_label_create(parent);
    lv_obj_set_style_text_font(center_date_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(center_date_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(center_date_label, LV_ALIGN_CENTER, 0, 25);
    lv_obj_add_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
    
    // 4. Crear un timer para actualizar la UI
    update_timer = lv_timer_create(update_center_clock_task, 1000, NULL);
    update_center_clock_task(update_timer); // Llamada inicial para establecer el estado correcto

    // 5. Añadir un evento de limpieza al objeto padre
    lv_obj_add_event_cb(parent, cleanup_event_cb, LV_EVENT_DELETE, NULL);

    // 6. Registrar los manejadores de botones.
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, NULL, true, nullptr);

    // --- MODIFICADO: Registrar manejadores de volumen ---
    // Un toque (TAP) no hace nada
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, NULL, true, nullptr);
    // Mantener presionado (LONG_PRESS_HOLD) ajusta el volumen
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_LONG_PRESS_HOLD, handle_volume_down, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_HOLD, handle_volume_up, true, nullptr);
}