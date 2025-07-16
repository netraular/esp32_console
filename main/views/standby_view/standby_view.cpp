#include "standby_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/power_manager/power_manager.h" // <-- AÑADIR: Incluir el nuevo manager
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "STANDBY_VIEW";

// --- Configuración y variables para el control de volumen ---
#define VOLUME_REPEAT_PERIOD_MS 200 // Repetir cada 200ms (5 veces por segundo).

static lv_timer_t *volume_up_timer = NULL;
static lv_timer_t *volume_down_timer = NULL;

// --- Variables para los widgets de la vista ---
static lv_obj_t *center_time_label;
static lv_obj_t *center_date_label;
static lv_obj_t *loading_label;
static lv_timer_t *update_timer = NULL;
static bool is_time_synced = false;

// --- Tarea para actualizar el reloj central ---
static void update_center_clock_task(lv_timer_t *timer) {
    // Comprobación de seguridad: si la etiqueta ya no existe, no hacer nada.
    if (!center_time_label || !center_date_label || !loading_label) {
        return;
    }

    if (wifi_manager_is_connected()) {
        if (!is_time_synced) {
            is_time_synced = true;
            lv_obj_add_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
        }
        char time_str[16];
        char date_str[64];
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
            lv_obj_clear_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(loading_label, "Connecting...");
    }
}

// --- Manejador del botón para ir al menú ---
static void handle_cancel_press(void *user_data) {
    ESP_LOGI(TAG, "CANCEL pressed, loading menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

// --- NUEVO: Manejador para entrar en modo de bajo consumo ---
static void handle_on_off_press(void* user_data) {
    ESP_LOGI(TAG, "ON/OFF button pressed, entering light sleep.");
    power_manager_enter_light_sleep();
}


// --- Funciones para el control de volumen ---

// Callback del timer para subir volumen repetidamente
static void volume_up_timer_cb(lv_timer_t *timer) {
    audio_manager_volume_up();
    status_bar_update_volume_display(); // Actualización instantánea de la UI
}

// Callback del timer para bajar volumen repetidamente
static void volume_down_timer_cb(lv_timer_t *timer) {
    audio_manager_volume_down();
    status_bar_update_volume_display(); // Actualización instantánea de la UI
}

// Se ejecuta al INICIO de una pulsación larga del botón DERECHO
static void handle_volume_up_long_press_start(void *user_data) {
    audio_manager_volume_up();
    status_bar_update_volume_display(); // Actualización instantánea de la UI
    if (volume_up_timer == NULL) { // Inicia el timer de repetición
        volume_up_timer = lv_timer_create(volume_up_timer_cb, VOLUME_REPEAT_PERIOD_MS, NULL);
    }
}

// Se ejecuta al INICIO de una pulsación larga del botón IZQUIERDO
static void handle_volume_down_long_press_start(void *user_data) {
    audio_manager_volume_down();
    status_bar_update_volume_display(); // Actualización instantánea de la UI
    if (volume_down_timer == NULL) { // Inicia el timer de repetición
        volume_down_timer = lv_timer_create(volume_down_timer_cb, VOLUME_REPEAT_PERIOD_MS, NULL);
    }
}

// Se ejecuta al SOLTAR el botón de volumen (derecho o izquierdo)
static void handle_volume_button_release(void *user_data) {
    // Detiene y elimina los timers de repetición si existen
    if (volume_up_timer) {
        lv_timer_del(volume_up_timer);
        volume_up_timer = NULL;
    }
    if (volume_down_timer) {
        lv_timer_del(volume_down_timer);
        volume_down_timer = NULL;
    }
}

// --- Callback para limpieza de recursos al destruir la vista ---
static void cleanup_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Standby view container is being deleted, cleaning up timers.");
        if (update_timer) {
            lv_timer_del(update_timer);
            update_timer = NULL;
        }
        // Asegurarse de limpiar los timers de volumen también
        handle_volume_button_release(NULL);
        
        // Limpiar punteros de widgets para evitar accesos accidentales
        center_time_label = NULL;
        center_date_label = NULL;
        loading_label = NULL;
        is_time_synced = false;
    }
}

// --- Función de creación de la vista ---
void standby_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Standby View");
    is_time_synced = false;

    // Inicializar punteros de timer a NULL
    volume_up_timer = NULL;
    volume_down_timer = NULL;

    // Crear un contenedor principal para toda la vista
    lv_obj_t *view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(view_container);
    lv_obj_set_size(view_container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(view_container);

    // 1. Crear la barra de estado como hija del contenedor
    status_bar_create(view_container);

    // 2. Crear etiqueta "Loading..." como hija del contenedor
    loading_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(loading_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(loading_label, "Loading...");
    lv_obj_center(loading_label);

    // 3. Crear reloj y fecha centrales como hijos del contenedor (inicialmente ocultos)
    center_time_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(center_time_label, &lv_font_montserrat_48, 0);
    lv_obj_align(center_time_label, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);

    center_date_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(center_date_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(center_date_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(center_date_label, LV_ALIGN_CENTER, 0, 25);
    lv_obj_add_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
    
    // 4. Crear timer para actualizar la UI del reloj
    update_timer = lv_timer_create(update_center_clock_task, 1000, NULL);
    update_center_clock_task(update_timer);

    // 5. Añadir evento de limpieza al CONTENEDOR
    lv_obj_add_event_cb(view_container, cleanup_event_cb, LV_EVENT_DELETE, NULL);

    // 6. Registrar manejadores de botones
    // --- MODIFICACIÓN: El botón ON/OFF ahora entra en modo de bajo consumo ---
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_TAP, handle_on_off_press, true, nullptr);

    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, NULL, true, nullptr);

    // Las pulsaciones cortas (TAP) en izquierda/derecha ya no hacen nada.
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,  BUTTON_EVENT_TAP, NULL, true, nullptr);
    
    // Se mantienen los manejadores para la pulsación larga (inicio y fin)
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, handle_volume_up_long_press_start, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,  BUTTON_EVENT_LONG_PRESS_START, handle_volume_down_long_press_start, true, nullptr);

    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_PRESS_UP, handle_volume_button_release, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,  BUTTON_EVENT_PRESS_UP, handle_volume_button_release, true, nullptr);
}