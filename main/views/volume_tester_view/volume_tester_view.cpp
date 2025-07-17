#include "volume_tester_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "config.h"
#include "esp_log.h"
#include <new> // Para std::nothrow

static const char* TAG = "VolumeTesterView";
static const char* TEST_SOUND_PATH = "/sdcard/sounds/test.wav";

// **NUEVO**: Estructura para el estado de la vista, encapsulando todos los elementos dinámicos.
typedef struct {
    lv_obj_t* volume_label;
    lv_obj_t* status_label;
    lv_timer_t* audio_check_timer;
    bool is_playing;
} volume_tester_data_t;

// --- Prototipos ---
static void update_volume_label(lv_obj_t* label);
static void audio_check_timer_cb(lv_timer_t* timer);
static void handle_ok_press(void* user_data);
static void handle_exit(void* user_data);
static void view_delete_cb(lv_event_t* e);

// Actualiza la etiqueta que muestra el porcentaje de volumen
static void update_volume_label(lv_obj_t* label) {
    if (label) {
        // Usamos get_volume() que devuelve el valor físico real (0-100)
        uint8_t vol = audio_manager_get_volume();
        lv_label_set_text_fmt(label, "%u%%", vol);
    }
}

// Sube el volumen y actualiza la UI
static void handle_volume_up(void* user_data) {
    auto* data = static_cast<volume_tester_data_t*>(user_data);
    audio_manager_volume_up();
    update_volume_label(data->volume_label);
}

// Baja el volumen y actualiza la UI
static void handle_volume_down(void* user_data) {
    auto* data = static_cast<volume_tester_data_t*>(user_data);
    audio_manager_volume_down();
    update_volume_label(data->volume_label);
}

// Sale de la vista y vuelve al menú principal
static void handle_exit(void* user_data) {
    // La limpieza se hará en el callback de DELETE, no aquí.
    view_manager_load_view(VIEW_ID_MENU);
}

// Timer para reiniciar la reproducción de audio y crear un bucle
static void audio_check_timer_cb(lv_timer_t* timer) {
    auto* data = static_cast<volume_tester_data_t*>(lv_timer_get_user_data(timer));
    audio_player_state_t state = audio_manager_get_state();
    if (state == AUDIO_STATE_STOPPED || state == AUDIO_STATE_ERROR) {
        ESP_LOGI(TAG, "Audio stopped, re-playing for loop effect.");
        if (!audio_manager_play(TEST_SOUND_PATH)) {
            if(data && data->status_label) {
                lv_label_set_text(data->status_label, "Error re-playing!");
            }
        }
    }
}

// Inicia o detiene la reproducción de audio al pulsar OK
static void handle_ok_press(void* user_data) {
    auto* data = static_cast<volume_tester_data_t*>(user_data);

    if (data->is_playing) {
        // --- Detener la reproducción ---
        ESP_LOGI(TAG, "OK pressed: Stopping playback.");
        audio_manager_stop();
        if (data->audio_check_timer) {
            lv_timer_delete(data->audio_check_timer);
            data->audio_check_timer = nullptr;
        }
        lv_label_set_text(data->status_label, "Press OK to Play");
        lv_obj_set_style_text_color(data->status_label, lv_color_white(), 0);
        data->is_playing = false;
    } else {
        // --- Iniciar la reproducción ---
        ESP_LOGI(TAG, "OK pressed: Starting playback.");
        if (audio_manager_play(TEST_SOUND_PATH)) {
            data->audio_check_timer = lv_timer_create(audio_check_timer_cb, 500, data);
            lv_label_set_text(data->status_label, "Playing...");
            lv_obj_set_style_text_color(data->status_label, lv_palette_main(LV_PALETTE_GREEN), 0);
            data->is_playing = true;
        } else {
            lv_label_set_text(data->status_label, "Error: Can't play file!");
            lv_obj_set_style_text_color(data->status_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }
}

// **NUEVO**: Callback de limpieza llamado cuando la vista se elimina
static void view_delete_cb(lv_event_t* e) {
    ESP_LOGI(TAG, "Cleaning up Volume Tester View resources.");
    auto* data = static_cast<volume_tester_data_t*>(lv_event_get_user_data(e));
    if (data) {
        // 1. Detener el timer si existe
        if (data->audio_check_timer) {
            lv_timer_delete(data->audio_check_timer);
        }
        // 2. Detener la reproducción de audio es crucial
        audio_manager_stop();
        // 3. Restaurar un volumen seguro por defecto al salir de esta vista de prueba
        audio_manager_set_volume_physical(MAX_VOLUME_PERCENTAGE > 15 ? 15 : MAX_VOLUME_PERCENTAGE);
        // 4. Liberar la memoria de la estructura de estado
        delete data;
    }
}

void volume_tester_view_create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Volume Tester View");

    // **NUEVO**: Usar C++ new/delete para la estructura de estado
    auto* data = new(std::nothrow) volume_tester_data_t;
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for view data");
        return;
    }
    // Inicializar el estado a un valor conocido
    data->audio_check_timer = nullptr;
    data->is_playing = false;
    data->volume_label = nullptr;
    data->status_label = nullptr;
    
    // **NUEVO**: Crear un contenedor principal para esta vista.
    lv_obj_t* view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(view_container);
    lv_obj_set_size(view_container, lv_pct(100), lv_pct(100));
    lv_obj_center(view_container);

    // **NUEVO**: Registrar el callback de limpieza en ESTE contenedor.
    lv_obj_add_event_cb(view_container, view_delete_cb, LV_EVENT_DELETE, data);

    // Aplicar el layout y los estilos al nuevo contenedor.
    lv_obj_set_flex_flow(view_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(view_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(view_container, 10, 0);
    lv_obj_set_style_pad_gap(view_container, 15, 0);

    // Crear todos los widgets como hijos del nuevo contenedor.
    lv_obj_t* title_label = lv_label_create(view_container);
    lv_label_set_text(title_label, "Volume Tester");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);

    data->volume_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(data->volume_label, &lv_font_montserrat_48, 0);
    update_volume_label(data->volume_label);

    data->status_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(data->status_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(data->status_label, "Press OK to Play");

    lv_obj_t* info_label = lv_label_create(view_container);
    lv_label_set_text(info_label,
                      "Find max safe volume.\n\n"
                      LV_SYMBOL_LEFT " / " LV_SYMBOL_RIGHT " : Adjust Volume\n"
                      "OK : Play / Stop\n"
                      "CANCEL : Exit");
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(info_label, 4, 0);

    // Registrar manejadores de botones, pasando el puntero a los datos de estado
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, handle_volume_down, true, data);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, handle_volume_up,   true, data);
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press,    true, data);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_exit,        true, data);
    
    // También manejar la pulsación larga para subir/bajar volumen más rápido
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_LONG_PRESS_HOLD, handle_volume_down, true, data);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_LONG_PRESS_HOLD, handle_volume_up,   true, data);
}