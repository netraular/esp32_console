#include "volume_tester_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "config.h"
#include "esp_log.h"

static const char* TAG = "VolumeTesterView";
// IMPORTANTE: Asegúrate de que este archivo WAV existe en tu tarjeta SD.
static const char* TEST_SOUND_PATH = "/sdcard/sounds/test.wav";

// Estructura de datos para mantener el estado de la vista
typedef struct {
    lv_obj_t* volume_label;
    lv_obj_t* status_label;
    lv_timer_t* audio_check_timer;
    bool is_playing;
} volume_tester_data_t;

// --- Prototipos de funciones ---
static void update_volume_label(lv_obj_t* label);
static void handle_ok_press(void* user_data);
static void audio_check_timer_cb(lv_timer_t* timer);

// Actualiza la etiqueta que muestra el porcentaje de volumen
static void update_volume_label(lv_obj_t* label) {
    if (label) {
        uint8_t vol = audio_manager_get_volume();
        // Aquí usamos el set_volume_physical que da el valor real 0-100
        lv_label_set_text_fmt(label, "%u%%", vol);
    }
}

// Sube el volumen y actualiza la UI
static void handle_volume_up(void* user_data) {
    volume_tester_data_t* data = (volume_tester_data_t*)user_data;
    uint8_t current_vol = audio_manager_get_volume();
    if (current_vol < 100) {
        audio_manager_set_volume_physical(current_vol + 1);
        update_volume_label(data->volume_label);
    }
}

// Baja el volumen y actualiza la UI
static void handle_volume_down(void* user_data) {
    volume_tester_data_t* data = (volume_tester_data_t*)user_data;
    uint8_t current_vol = audio_manager_get_volume();
    if (current_vol > 0) {
        audio_manager_set_volume_physical(current_vol - 1);
        update_volume_label(data->volume_label);
    }
}

// Sale de la vista y vuelve al menú principal
static void handle_exit(void* user_data) {
    // No es necesario detener el audio aquí, el callback de DELETE se encargará.
    view_manager_load_view(VIEW_ID_MENU);
}

// Timer para reiniciar la reproducción de audio y crear un bucle
static void audio_check_timer_cb(lv_timer_t* timer) {
    audio_player_state_t state = audio_manager_get_state();
    if (state == AUDIO_STATE_STOPPED || state == AUDIO_STATE_ERROR) {
        ESP_LOGI(TAG, "Audio stopped, re-playing for loop effect.");
        audio_manager_play(TEST_SOUND_PATH);
    }
}

// Inicia o detiene la reproducción de audio al pulsar OK
static void handle_ok_press(void* user_data) {
    volume_tester_data_t* data = (volume_tester_data_t*)user_data;

    if (data->is_playing) {
        // --- Detener la reproducción ---
        ESP_LOGI(TAG, "OK pressed: Stopping playback.");
        audio_manager_stop();
        if (data->audio_check_timer) {
            lv_timer_delete(data->audio_check_timer);
            data->audio_check_timer = NULL;
        }
        lv_label_set_text(data->status_label, "Press OK to Play");
        lv_obj_set_style_text_color(data->status_label, lv_color_black(), 0);
        data->is_playing = false;
    } else {
        // --- Iniciar la reproducción ---
        ESP_LOGI(TAG, "OK pressed: Starting playback.");
        if (audio_manager_play(TEST_SOUND_PATH)) {
            data->audio_check_timer = lv_timer_create(audio_check_timer_cb, 500, NULL);
            lv_label_set_text(data->status_label, "Playing...");
            lv_obj_set_style_text_color(data->status_label, lv_palette_main(LV_PALETTE_GREEN), 0);
            data->is_playing = true;
        } else {
            lv_label_set_text(data->status_label, "Error: Can't play file!");
            lv_obj_set_style_text_color(data->status_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }
}

// Callback de limpieza llamado cuando la vista se elimina
static void view_delete_cb(lv_event_t* e) {
    ESP_LOGI(TAG, "Cleaning up Volume Tester View.");
    volume_tester_data_t* data = (volume_tester_data_t*)lv_event_get_user_data(e);
    if (data) {
        if (data->audio_check_timer) {
            lv_timer_delete(data->audio_check_timer);
        }
        // Detener el audio al salir de la vista es crucial
        audio_manager_stop();
        // Restaurar un volumen seguro por defecto al salir
        audio_manager_set_volume_physical(MAX_VOLUME_PERCENTAGE > 5 ? 5 : MAX_VOLUME_PERCENTAGE);
        lv_free(data);
    }
}

void volume_tester_view_create(lv_obj_t* parent) {
    volume_tester_data_t* data = (volume_tester_data_t*)lv_malloc(sizeof(volume_tester_data_t));
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for view data");
        return;
    }
    // Inicializar el estado
    data->audio_check_timer = NULL;
    data->is_playing = false;
    
    // --- INICIO DE LA CORRECCIÓN ---
    
    // 1. Crear un contenedor principal para esta vista.
    lv_obj_t* view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(view_container);
    lv_obj_set_size(view_container, lv_pct(100), lv_pct(100));
    lv_obj_center(view_container);

    // 2. Registrar el callback de limpieza en ESTE contenedor, no en el padre.
    lv_obj_add_event_cb(view_container, view_delete_cb, LV_EVENT_DELETE, data);

    // 3. Aplicar el layout y los estilos al nuevo contenedor.
    lv_obj_set_flex_flow(view_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(view_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(view_container, 10, 0);
    lv_obj_set_style_pad_gap(view_container, 15, 0);

    // 4. Crear todos los widgets como hijos del nuevo contenedor.
    // Título de la vista
    lv_obj_t* title_label = lv_label_create(view_container);
    lv_label_set_text(title_label, "Volume Tester");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);

    // Etiqueta grande para el volumen actual
    data->volume_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(data->volume_label, &lv_font_montserrat_48, 0);
    update_volume_label(data->volume_label);

    // Etiqueta para el estado (Reproduciendo / Pausado)
    data->status_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(data->status_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(data->status_label, "Press OK to Play");

    // Etiqueta con las instrucciones
    lv_obj_t* info_label = lv_label_create(view_container);
    lv_label_set_text(info_label,
                      "Find the max safe volume.\n\n"
                      LV_SYMBOL_LEFT " / " LV_SYMBOL_RIGHT " : Adjust Volume\n"
                      "OK : Play / Stop Tone\n"
                      LV_SYMBOL_CLOSE " : Exit to Menu");
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(info_label, 4, 0);

    // --- FIN DE LA CORRECCIÓN ---

    // Registrar manejadores de botones
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP,             handle_volume_down, true, data);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP,             handle_volume_up,   true, data);
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP,             handle_ok_press,    true, data);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP,             handle_exit,        true, data);
}