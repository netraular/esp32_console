#include "speaker_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "../file_explorer/file_explorer.h"
#include "esp_log.h"
#include <string.h>
#include "components/audio_visualizer/audio_visualizer.h"
#include "config.h" // <-- [FIX] Añadir include para MAX_VOLUME_PERCENTAGE

static const char *TAG = "SPEAKER_TEST_VIEW";

// --- Variables de estado de la vista ---
static lv_obj_t *view_parent = NULL;
static lv_obj_t *play_pause_btn_label = NULL;
static char current_song_path[256];

// --- Widgets de la UI ---
static lv_obj_t *slider_widget = NULL;
static lv_obj_t *time_current_label_widget = NULL;
static lv_obj_t *time_total_label_widget = NULL;
static lv_timer_t *ui_update_timer = NULL;
static lv_obj_t *volume_label_widget = NULL;
static lv_obj_t *visualizer_widget = NULL;
static visualizer_data_t current_viz_data;
static bool viz_data_received = false;

// --- Prototipos de funciones worker para lv_async_call ---
static void update_volume_label_task(void *user_data);
static void handle_play_pause_ui_update_task(void *user_data);

// --- Prototipos del resto de funciones ---
static void create_initial_speaker_view();
static void show_file_explorer();
static void create_now_playing_view(const char *file_path);
static void handle_ok_press_playing();
static void handle_cancel_press_playing();
static void on_audio_file_selected(const char *path);
static void on_explorer_exit_speaker();
static void handle_ok_press_initial();
static void handle_cancel_press_initial();
static void ui_update_timer_cb(lv_timer_t *timer);
static void handle_left_press_playing();
static void handle_right_press_playing();


// --- Funciones de utilidad ---
static bool is_wav_file(const char *path) {
    const char *ext = strrchr(path, '.');
    return ext && (strcasecmp(ext, ".wav") == 0);
}

static const char* get_filename_from_path(const char* path) {
    const char* filename = strrchr(path, '/');
    return filename ? filename + 1 : path;
}

static void format_time(char *buf, size_t buf_size, uint32_t time_s) {
    snprintf(buf, buf_size, "%02lu:%02lu", time_s / 60, time_s % 60);
}

// --- Timer UI (ya se ejecuta en el contexto correcto) ---
static void ui_update_timer_cb(lv_timer_t *timer) {
    audio_player_state_t state = audio_manager_get_state();
    
    if (state == AUDIO_STATE_STOPPED) {
        handle_play_pause_ui_update_task(NULL); // Actualiza el icono de play/pausa
        if (slider_widget) lv_slider_set_value(slider_widget, 0, LV_ANIM_OFF);
        if (time_current_label_widget) lv_label_set_text(time_current_label_widget, "00:00");
        
        if (visualizer_widget) {
            uint8_t zero_values[VISUALIZER_BAR_COUNT] = {0};
            audio_visualizer_set_values(visualizer_widget, zero_values);
        }

        if (ui_update_timer) {
            lv_timer_delete(ui_update_timer);
            ui_update_timer = NULL;
        }
        return;
    }

    uint32_t duration = audio_manager_get_duration_s();
    uint32_t progress = audio_manager_get_progress_s();

    if (slider_widget && duration > 0 && lv_slider_get_max_value(slider_widget) != duration) {
        char time_buf[8];
        format_time(time_buf, sizeof(time_buf), duration);
        lv_label_set_text(time_total_label_widget, time_buf);
        lv_slider_set_range(slider_widget, 0, duration);
    }

    if (time_current_label_widget) {
        char time_buf[8];
        format_time(time_buf, sizeof(time_buf), progress);
        lv_label_set_text(time_current_label_widget, time_buf);
    }
    if (slider_widget) {
        lv_slider_set_value(slider_widget, progress, LV_ANIM_OFF);
    }

   QueueHandle_t queue = audio_manager_get_visualizer_queue();
    if (queue && visualizer_widget) {
        if (xQueueReceive(queue, &current_viz_data, 0) == pdPASS) {
            viz_data_received = true;
            audio_visualizer_set_values(visualizer_widget, current_viz_data.bar_values);
        } else {
            if(viz_data_received) {
                bool changed = false;
                for(int i = 0; i < VISUALIZER_BAR_COUNT; i++) {
                    if (current_viz_data.bar_values[i] > 0) {
                        // [CAMBIO] Reducimos la "gravedad" para una caída más suave
                        int new_val = (int)current_viz_data.bar_values[i] - 8; 
                        current_viz_data.bar_values[i] = (new_val < 0) ? 0 : (uint8_t)new_val;
                        changed = true;
                    }
                }
                if (changed) {
                    audio_visualizer_set_values(visualizer_widget, current_viz_data.bar_values);
                } else {
                    viz_data_received = false;
                }
            }
        }
    }
}


// --- [FIX] Funciones "Worker" para lv_async_call ---
// Esta es la función que realmente actualiza la UI del volumen.
// Se llamará de forma segura desde el lv_timer_handler.
static void update_volume_label_task(void *user_data) {
    if (volume_label_widget) {
        char vol_buf[16];
        uint8_t vol = audio_manager_get_volume();
        // Muestra el porcentaje relativo al máximo permitido (0-100%)
        uint8_t display_vol = (vol * 100) / MAX_VOLUME_PERCENTAGE;
        const char * vol_icon = (vol == 0) ? LV_SYMBOL_MUTE : (vol < (MAX_VOLUME_PERCENTAGE / 2)) ? LV_SYMBOL_VOLUME_MID : LV_SYMBOL_VOLUME_MAX;
        
        snprintf(vol_buf, sizeof(vol_buf), "%s %u%%", vol_icon, display_vol);
        lv_label_set_text(volume_label_widget, vol_buf);
    }
}

// Esta función actualiza el icono de play/pausa
static void handle_play_pause_ui_update_task(void *user_data) {
    audio_player_state_t state = audio_manager_get_state();
    if (play_pause_btn_label) {
        if (state == AUDIO_STATE_PLAYING) {
            lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PAUSE);
        } else {
            lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PLAY);
        }
    }
}

// --- Handlers de botones ---

static void handle_ok_press_playing() {
    audio_player_state_t state = audio_manager_get_state();
    if (state == AUDIO_STATE_STOPPED) {
        if (audio_manager_play(current_song_path)) {
            lv_async_call(handle_play_pause_ui_update_task, NULL); // <-- [FIX] Llamada asíncrona
            if (ui_update_timer == NULL) {
                ui_update_timer = lv_timer_create(ui_update_timer_cb, 50, NULL);
            }
        }
    } else if (state == AUDIO_STATE_PLAYING) {
        audio_manager_pause();
        lv_async_call(handle_play_pause_ui_update_task, NULL); // <-- [FIX] Llamada asíncrona
    } else if (state == AUDIO_STATE_PAUSED) {
        audio_manager_resume();
        lv_async_call(handle_play_pause_ui_update_task, NULL); // <-- [FIX] Llamada asíncrona
    }
}

static void handle_cancel_press_playing() {
    audio_manager_stop(); 
    play_pause_btn_label = NULL;
    volume_label_widget = NULL;
    visualizer_widget = NULL;
    show_file_explorer();
}

static void handle_left_press_playing() {
    audio_manager_volume_down();
    lv_async_call(update_volume_label_task, NULL); // <-- [FIX] Llamada asíncrona en lugar de directa
}

static void handle_right_press_playing() {
    audio_manager_volume_up();
    lv_async_call(update_volume_label_task, NULL); // <-- [FIX] Llamada asíncrona en lugar de directa
}

// --- Resto de funciones (sin cambios en su lógica interna) ---

static void on_audio_file_selected(const char *path) {
    if (is_wav_file(path)) {
        file_explorer_destroy();
        create_now_playing_view(path);
    }
}

static void on_explorer_exit_speaker() {
    file_explorer_destroy(); 
    lv_obj_clean(view_parent);
    create_initial_speaker_view(); 
}

static void show_file_explorer() {
    if (!sd_manager_is_mounted()) return;
    lv_obj_clean(view_parent);
    lv_obj_t * explorer_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(100), lv_pct(100));
    lv_obj_center(explorer_container);
    file_explorer_create(explorer_container, sd_manager_get_mount_point(), on_audio_file_selected, on_explorer_exit_speaker);
}

static void create_now_playing_view(const char *file_path) {
    lv_obj_clean(view_parent);
    strncpy(current_song_path, file_path, sizeof(current_song_path) - 1);
    viz_data_received = false;

    lv_obj_t *main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *top_cont = lv_obj_create(main_cont);
    lv_obj_remove_style_all(top_cont);
    lv_obj_set_size(top_cont, lv_pct(95), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *title_label = lv_label_create(top_cont);
    lv_label_set_text(title_label, get_filename_from_path(file_path));
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(title_label, lv_pct(65));
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_LEFT, 0);

    volume_label_widget = lv_label_create(top_cont);
    update_volume_label_task(NULL); // Llamada inicial segura
    lv_obj_set_style_text_font(volume_label_widget, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(volume_label_widget, LV_TEXT_ALIGN_RIGHT, 0);

    visualizer_widget = audio_visualizer_create(main_cont, VISUALIZER_BAR_COUNT);
    lv_obj_set_size(visualizer_widget, lv_pct(85), lv_pct(40));
    
    lv_obj_t *progress_cont = lv_obj_create(main_cont);
    lv_obj_remove_style_all(progress_cont);
    lv_obj_set_width(progress_cont, lv_pct(95));
    lv_obj_set_height(progress_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(progress_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(progress_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    time_current_label_widget = lv_label_create(progress_cont);
    lv_label_set_text(time_current_label_widget, "00:00");
    
    slider_widget = lv_slider_create(progress_cont);
    lv_obj_set_style_flex_grow(slider_widget, 1, 0);
    lv_obj_remove_flag(slider_widget, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_margin_hor(slider_widget, 10, 0);

    time_total_label_widget = lv_label_create(progress_cont);
    lv_label_set_text(time_total_label_widget, "??:??");

    lv_obj_t *play_pause_btn = lv_button_create(main_cont);
    play_pause_btn_label = lv_label_create(play_pause_btn);
    lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(play_pause_btn_label, &lv_font_montserrat_28, 0);
    
    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_playing);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_playing);
    button_manager_register_view_handler(BUTTON_LEFT, handle_left_press_playing);
    button_manager_register_view_handler(BUTTON_RIGHT, handle_right_press_playing);

    handle_ok_press_playing();
}

static void handle_ok_press_initial() {
    show_file_explorer();
}

static void handle_cancel_press_initial() {
    if (ui_update_timer) {
        lv_timer_delete(ui_update_timer);
        ui_update_timer = NULL;
    }
    audio_manager_stop(); 
    view_manager_load_view(VIEW_ID_MENU);
}

static void create_initial_speaker_view() {
    lv_obj_t *label = lv_label_create(view_parent);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_label_set_text(label, "Test Speaker");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *info_label = lv_label_create(view_parent);
    lv_label_set_text(info_label, "Presiona OK para\nseleccionar un archivo de audio");
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label);

    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_initial);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_initial);
}

void speaker_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Speaker Test View");
    view_parent = parent;
    create_initial_speaker_view();
}