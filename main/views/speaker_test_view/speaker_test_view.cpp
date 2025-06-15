#include "speaker_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "../file_explorer/file_explorer.h"
#include "esp_log.h"
#include <string.h>
#include "components/audio_visualizer/audio_visualizer.h"
#include "config.h"
#include <math.h>

static const char *TAG = "SPEAKER_TEST_VIEW";

// --- Variables de estado de la vista ---
static lv_obj_t *view_parent = NULL;
static lv_obj_t *play_pause_btn_label = NULL;
static char current_song_path[256];
static bool is_exiting = false;

// --- Widgets de la UI ---
static lv_obj_t *slider_widget = NULL;
static lv_obj_t *time_current_label_widget = NULL;
static lv_obj_t *time_total_label_widget = NULL;
static lv_timer_t *ui_update_timer = NULL;
static lv_obj_t *volume_label_widget = NULL;
static lv_obj_t *visualizer_widget = NULL;
static lv_obj_t *info_label_widget = NULL;
static visualizer_data_t current_viz_data;
static bool viz_data_received = false;

// --- Prototipos de funciones worker ---
static void update_volume_label_task(void *user_data);
static void handle_play_pause_ui_update_task(void *user_data);
static void stop_audio_task(void* user_data);

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
static void cleanup_player_and_return_to_initial_view();

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

// --- Timer UI ---
static void ui_update_timer_cb(lv_timer_t *timer) {
    audio_player_state_t state = audio_manager_get_state();
    
    // Si se solicitó salir y el manager confirma que está parado, limpiamos la UI.
    if (is_exiting && state == AUDIO_STATE_STOPPED) {
        ESP_LOGI(TAG, "Audio manager confirmed STOPPED state. Cleaning up UI.");
        cleanup_player_and_return_to_initial_view();
        return; // Detenemos el procesamiento del timer para este ciclo.
    }
    
    if (state == AUDIO_STATE_ERROR) {
        ESP_LOGW(TAG, "Audio manager reported an error. Unmounting SD and returning to initial view.");
        sd_manager_unmount(); 
        cleanup_player_and_return_to_initial_view();
        return;
    }

    // Si la canción termina por sí misma
    if (!is_exiting && state == AUDIO_STATE_STOPPED) {
        ESP_LOGI(TAG, "Song finished naturally. Resetting UI to initial playback state.");
        handle_play_pause_ui_update_task(NULL);
        if (slider_widget) lv_slider_set_value(slider_widget, 0, LV_ANIM_OFF);
        if (time_current_label_widget) lv_label_set_text(time_current_label_widget, "00:00");
        
        if (visualizer_widget) {
            uint8_t zero_values[VISUALIZER_BAR_COUNT] = {0};
            audio_visualizer_set_values(visualizer_widget, zero_values);
        }
        
        // Detenemos el timer para ahorrar CPU, se reactivará si se pulsa play.
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


// --- Funciones "Worker" para lv_async_call ---
static void update_volume_label_task(void *user_data) {
    if (volume_label_widget) {
        char vol_buf[16];
        uint8_t vol = audio_manager_get_volume();
        uint8_t raw_display_vol = (vol * 100) / MAX_VOLUME_PERCENTAGE;
        const int volume_step = 5;
        uint8_t display_vol = (uint8_t)(roundf((float)raw_display_vol / volume_step) * volume_step);
        const char * vol_icon = (display_vol == 0) ? LV_SYMBOL_MUTE : (vol < (MAX_VOLUME_PERCENTAGE / 2)) ? LV_SYMBOL_VOLUME_MID : LV_SYMBOL_VOLUME_MAX;
        snprintf(vol_buf, sizeof(vol_buf), "%s %u%%", vol_icon, display_vol);
        lv_label_set_text(volume_label_widget, vol_buf);
    }
}

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

// Tarea asíncrona para detener el audio de forma segura
static void stop_audio_task(void* user_data) {
    // Esta llamada es ahora segura porque no se ejecuta en un callback crítico.
    audio_manager_stop();
}

// --- Handlers de botones ---

static void handle_ok_press_playing() {
    audio_player_state_t state = audio_manager_get_state();
    if (state == AUDIO_STATE_STOPPED) {
        // Si estamos saliendo, no permitir iniciar una nueva reproducción
        if (is_exiting) return;
        if (audio_manager_play(current_song_path)) {
            lv_async_call(handle_play_pause_ui_update_task, NULL);
            if (ui_update_timer == NULL) {
                ui_update_timer = lv_timer_create(ui_update_timer_cb, 50, NULL);
            }
        } else {
            ESP_LOGE(TAG, "Failed to start audio playback. Returning to initial view.");
            cleanup_player_and_return_to_initial_view();
        }
    } else if (state == AUDIO_STATE_PLAYING) {
        audio_manager_pause();
        lv_async_call(handle_play_pause_ui_update_task, NULL);
    } else if (state == AUDIO_STATE_PAUSED) {
        audio_manager_resume();
        lv_async_call(handle_play_pause_ui_update_task, NULL);
    }
}

static void handle_cancel_press_playing() {
    if (is_exiting) return; // Prevenir múltiples llamadas

    ESP_LOGI(TAG, "Cancel pressed. Requesting audio stop asynchronously.");
    is_exiting = true;
    button_manager_unregister_view_handlers(); // Desactivar botones inmediatamente
    
    // Llamar a la parada de forma asíncrona para no bloquear el timer del botón
    lv_async_call(stop_audio_task, NULL);
}

static void handle_left_press_playing() {
    audio_manager_volume_down();
    lv_async_call(update_volume_label_task, NULL);
}

static void handle_right_press_playing() {
    audio_manager_volume_up();
    lv_async_call(update_volume_label_task, NULL);
}

// --- Resto de funciones ---

static void cleanup_player_and_return_to_initial_view() {
    if (ui_update_timer) {
        lv_timer_delete(ui_update_timer);
        ui_update_timer = NULL;
    }
    // audio_manager_stop() ya se ha llamado de forma asíncrona.
    
    play_pause_btn_label = NULL;
    volume_label_widget = NULL;
    visualizer_widget = NULL;
    slider_widget = NULL;
    time_current_label_widget = NULL;
    time_total_label_widget = NULL;
    
    lv_obj_clean(view_parent);
    create_initial_speaker_view();
}

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
    lv_obj_clean(view_parent);
    lv_obj_t * explorer_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(100), lv_pct(100));
    lv_obj_center(explorer_container);
    file_explorer_create(explorer_container, sd_manager_get_mount_point(), on_audio_file_selected, on_explorer_exit_speaker);
}

static void create_now_playing_view(const char *file_path) {
    is_exiting = false; // Resetear el estado al crear la vista
    lv_obj_clean(view_parent);
    strncpy(current_song_path, file_path, sizeof(current_song_path) - 1);
    viz_data_received = false;

    // ... (el resto del código de creación de la UI no cambia)
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
    update_volume_label_task(NULL);
    lv_obj_set_style_text_font(volume_label_widget, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(volume_label_widget, LV_TEXT_ALIGN_RIGHT, 0);

    visualizer_widget = audio_visualizer_create(main_cont, VISUALIZER_BAR_COUNT);
    lv_obj_set_size(visualizer_widget, lv_pct(100), lv_pct(40));
    
    lv_obj_t *progress_cont = lv_obj_create(main_cont);
    lv_obj_remove_style_all(progress_cont);
    lv_obj_set_size(progress_cont, lv_pct(95), LV_SIZE_CONTENT);
    lv_obj_clear_flag(progress_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_top(progress_cont, 10, 0); 

    slider_widget = lv_slider_create(progress_cont);
    lv_obj_remove_flag(slider_widget, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(slider_widget, lv_pct(100));
    lv_obj_align(slider_widget, LV_ALIGN_TOP_MID, 0, 0);

    lv_color_t indicator_color = lv_palette_main(LV_PALETTE_BLUE);
    lv_obj_set_style_bg_color(slider_widget, indicator_color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_widget, lv_color_mix(indicator_color, lv_color_black(), LV_OPA_50), LV_PART_KNOB);

    lv_obj_set_style_radius(slider_widget, 4, LV_PART_KNOB);

    time_current_label_widget = lv_label_create(progress_cont);
    lv_label_set_text(time_current_label_widget, "00:00");
    lv_obj_align_to(time_current_label_widget, slider_widget, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    time_total_label_widget = lv_label_create(progress_cont);
    lv_label_set_text(time_total_label_widget, "??:??");
    lv_obj_align_to(time_total_label_widget, slider_widget, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 5);


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
    ESP_LOGI(TAG, "Botón OK presionado. Forzando re-montaje de la SD...");
    
    sd_manager_unmount();

    if (sd_manager_mount()) {
        ESP_LOGI(TAG, "Re-montaje exitoso. Abriendo el explorador de archivos.");
        show_file_explorer();
    } else {
        ESP_LOGW(TAG, "El re-montaje de la SD falló. Mostrando mensaje de error.");
        if (info_label_widget) {
            lv_label_set_text(info_label_widget, "Fallo al leer la SD.\n\n"
                                                 "Revise la tarjeta y\n"
                                                 "presione OK para reintentar.");
        }
    }
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

    info_label_widget = lv_label_create(view_parent);
    lv_obj_set_style_text_align(info_label_widget, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label_widget);

    lv_label_set_text(info_label_widget, "Presiona OK para\nseleccionar un archivo de audio");

    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_initial);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_initial);
}

void speaker_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Speaker Test View");
    view_parent = parent;
    create_initial_speaker_view();
}