#include "click_counter_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "esp_log.h"

static const char *TAG = "CLICK_COUNTER_VIEW";

// --- Variables de estado de la vista ---
static lv_obj_t* count_label;
static int click_count = 0;
static const char* SOUND_FILE_PATH = "/sdcard/sounds/bright_earn.wav";

// --- Funciones internas ---

// Actualiza el texto de la etiqueta con el contador actual
static void update_counter_label() {
    if (count_label) {
        lv_label_set_text_fmt(count_label, "%d", click_count);
    }
}

// --- Manejadores de botones ---

// Se ejecuta al presionar el botón OK
static void handle_ok_press(void* user_data) {
    click_count++;
    update_counter_label();

    // Comprueba si el contador es un múltiplo de 10
    if (click_count > 0 && click_count % 10 == 0) {
        ESP_LOGI(TAG, "Contador alcanzó %d, reproduciendo sonido.", click_count);
        // El audio_manager se detendrá si algo más se está reproduciendo, lo cual es perfecto para este efecto de sonido corto.
        audio_manager_play(SOUND_FILE_PATH);
    }
}

// Se ejecuta al presionar el botón Cancel
static void handle_cancel_press(void* user_data) {
    // Detiene cualquier sonido que pudiera haber quedado reproduciéndose
    audio_manager_stop();
    // Vuelve al menú principal
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Función pública para crear la vista ---

void click_counter_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creando la vista del contador de clics");
    
    // Reinicia el contador cada vez que se crea la vista
    click_count = 0;

    // Crea una etiqueta para el título
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Click Counter");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    // Crea la etiqueta principal para mostrar el número
    count_label = lv_label_create(parent);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_48, 0);
    lv_obj_center(count_label);
    update_counter_label(); // Muestra el valor inicial (0)

    // Registra los manejadores de botones para esta vista
    // Solo OK y Cancel tienen acciones.
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, NULL, true, nullptr);
}