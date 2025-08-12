#include "audio_player_component.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "components/audio_visualizer/audio_visualizer.h"
#include "config/board_config.h"
#include "config/app_config.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "AUDIO_PLAYER_COMP";

// Component state data structure
typedef struct {
    char current_song_path[256];
    audio_player_exit_callback_t on_exit_cb;
    void* exit_cb_user_data;
    bool is_exiting;
    bool is_playing_active;
    bool viz_data_received;

    // UI Widgets
    lv_obj_t *play_pause_btn_label;
    lv_obj_t *slider_widget;
    lv_obj_t *time_current_label_widget;
    lv_obj_t *time_total_label_widget;
    lv_obj_t *volume_label_widget;
    lv_obj_t *visualizer_widget;
    visualizer_data_t audio_spectrum_data;
    lv_timer_t *ui_update_timer;
} audio_player_data_t;


// --- Internal Prototypes ---
static void update_volume_label(audio_player_data_t* data);
static void ui_update_timer_cb(lv_timer_t *timer);
static void player_container_delete_cb(lv_event_t * e);
static const char* get_filename_from_path(const char* path);

// --- Button Handlers (now use the component's user_data) ---
static void handle_ok_press(void* user_data) {
    audio_player_data_t* data = (audio_player_data_t*)user_data;
    audio_player_state_t state = audio_manager_get_state();
    switch (state) {
        case AUDIO_STATE_PLAYING: audio_manager_pause(); break;
        case AUDIO_STATE_PAUSED: audio_manager_resume(); break;
        case AUDIO_STATE_STOPPED:
        case AUDIO_STATE_ERROR: 
             audio_manager_play(data->current_song_path);
             data->is_playing_active = true;
             break;
    }
    if (data->play_pause_btn_label) {
        lv_label_set_text(data->play_pause_btn_label, (audio_manager_get_state() == AUDIO_STATE_PLAYING) ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }
}

static void handle_cancel_press(void* user_data) {
    audio_player_data_t* data = (audio_player_data_t*)user_data;
    if (data->is_exiting) return;
    data->is_exiting = true;
    button_manager_unregister_view_handlers();
    audio_manager_stop();
}

static void handle_volume_up(void* user_data) {
    audio_player_data_t* data = (audio_player_data_t*)user_data;
    audio_manager_volume_up();
    update_volume_label(data);
}

static void handle_volume_down(void* user_data) {
    audio_player_data_t* data = (audio_player_data_t*)user_data;
    audio_manager_volume_down();
    update_volume_label(data);
}

// --- UI Helper ---
static void update_volume_label(audio_player_data_t* data) {
    if (!data || !data->volume_label_widget) return;
    
    uint8_t physical_vol = audio_manager_get_volume();
    // The display volume is a 0-100 scale, mapped from the physical volume (0-MAX_VOLUME_PERCENTAGE)
    uint8_t raw_display_vol = (physical_vol * 100) / MAX_VOLUME_PERCENTAGE;
    // Round to nearest 5 for a stepped display
    uint8_t display_vol = (uint8_t)(roundf((float)raw_display_vol / 5.0f) * 5.0f);
    
    const char * icon;
    if (display_vol == 0) {
        icon = LV_SYMBOL_MUTE;
    } else if (display_vol < 50) {
        icon = LV_SYMBOL_VOLUME_MID;
    } else {
        icon = LV_SYMBOL_VOLUME_MAX;
    }
    
    lv_label_set_text_fmt(data->volume_label_widget, "%s %u%%", icon, display_vol);
}


// --- Main Create Function ---
lv_obj_t* audio_player_component_create(lv_obj_t* parent, const char* file_path, audio_player_exit_callback_t on_exit, void* exit_cb_user_data) {
    ESP_LOGI(TAG, "Creating for file: %s", file_path);
    
    audio_player_data_t* data = (audio_player_data_t*)lv_malloc(sizeof(audio_player_data_t));
    LV_ASSERT_MALLOC(data);
    if (!data) return NULL;
    memset(data, 0, sizeof(audio_player_data_t));

    strncpy(data->current_song_path, file_path, sizeof(data->current_song_path) - 1);
    data->on_exit_cb = on_exit;
    data->exit_cb_user_data = exit_cb_user_data;

    lv_obj_t *main_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(main_cont, player_container_delete_cb, LV_EVENT_DELETE, data);
    lv_obj_set_user_data(main_cont, data);

    // --- Top UI (Title & Volume) ---
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

    data->volume_label_widget = lv_label_create(top_cont);
    lv_obj_set_style_text_font(data->volume_label_widget, &lv_font_montserrat_16, 0);
    update_volume_label(data); // Call once to set initial value without changing it.

    // --- Visualizer ---
    data->visualizer_widget = audio_visualizer_create(main_cont, VISUALIZER_BAR_COUNT);
    lv_obj_set_size(data->visualizer_widget, lv_pct(100), lv_pct(40));
    
    // --- Progress Bar ---
    lv_obj_t *progress_cont = lv_obj_create(main_cont);
    lv_obj_remove_style_all(progress_cont);
    lv_obj_set_size(progress_cont, lv_pct(95), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(progress_cont, 10, 0); 
    data->slider_widget = lv_slider_create(progress_cont);
    lv_obj_remove_flag(data->slider_widget, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(data->slider_widget, lv_pct(100));
    data->time_current_label_widget = lv_label_create(progress_cont);
    lv_label_set_text(data->time_current_label_widget, "00:00");
    lv_obj_align_to(data->time_current_label_widget, data->slider_widget, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    data->time_total_label_widget = lv_label_create(progress_cont);
    lv_label_set_text(data->time_total_label_widget, "??:??");
    lv_obj_align_to(data->time_total_label_widget, data->slider_widget, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 5);

    // --- Play/Pause Button ---
    lv_obj_t *play_pause_btn = lv_button_create(main_cont);
    data->play_pause_btn_label = lv_label_create(play_pause_btn);
    lv_obj_set_style_text_font(data->play_pause_btn_label, &lv_font_montserrat_28, 0);
    
    // --- Logic Start ---
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press, true, data);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, data);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, handle_volume_down, true, data);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, handle_volume_up, true, data);
    
    if (audio_manager_play(data->current_song_path)) {
        data->is_playing_active = true;
        lv_label_set_text(data->play_pause_btn_label, LV_SYMBOL_PAUSE);
        data->ui_update_timer = lv_timer_create(ui_update_timer_cb, 50, data);
    } else {
        ESP_LOGE(TAG, "Failed to start audio playback.");
        lv_label_set_text(data->play_pause_btn_label, LV_SYMBOL_WARNING);
    }
    
    return main_cont;
}

// --- Timer Logic and Cleanup ---
static void ui_update_timer_cb(lv_timer_t *timer) {
    audio_player_data_t* data = (audio_player_data_t*)lv_timer_get_user_data(timer);

    audio_player_state_t state = audio_manager_get_state();

    if ((data->is_exiting && state == AUDIO_STATE_STOPPED) || state == AUDIO_STATE_ERROR) {
        if (data->on_exit_cb) {
            data->on_exit_cb(data->exit_cb_user_data); 
        }
        return; 
    }

    if (data->is_playing_active && state == AUDIO_STATE_STOPPED) {
        data->is_playing_active = false;
        lv_label_set_text(data->play_pause_btn_label, LV_SYMBOL_PLAY);
        lv_slider_set_value(data->slider_widget, 0, LV_ANIM_OFF);
        lv_label_set_text(data->time_current_label_widget, "00:00");
    }

    if (state == AUDIO_STATE_PLAYING || state == AUDIO_STATE_PAUSED) {
        uint32_t duration = audio_manager_get_duration_s();
        uint32_t progress = audio_manager_get_progress_s();
        if (duration > 0 && lv_slider_get_max_value(data->slider_widget) != duration) {
            lv_slider_set_range(data->slider_widget, 0, duration);
            lv_label_set_text_fmt(data->time_total_label_widget, "%02lu:%02lu", duration / 60, duration % 60);
        }
        lv_label_set_text_fmt(data->time_current_label_widget, "%02lu:%02lu", progress / 60, progress % 60);
        lv_slider_set_value(data->slider_widget, progress, LV_ANIM_OFF);

        QueueHandle_t queue = audio_manager_get_visualizer_queue();
        if (queue && xQueueReceive(queue, &data->audio_spectrum_data, 0) == pdPASS) {
            data->viz_data_received = true;
            audio_visualizer_set_values(data->visualizer_widget, data->audio_spectrum_data.bar_values);
        }
    }
}

static void player_container_delete_cb(lv_event_t * e) {
    audio_player_data_t* data = (audio_player_data_t*)lv_event_get_user_data(e);
    if (data) {
        ESP_LOGI(TAG, "Cleaning up audio player component.");
        if (data->ui_update_timer) {
            lv_timer_delete(data->ui_update_timer);
        }
        // Ensure audio stops if it hasn't already
        if (audio_manager_get_state() != AUDIO_STATE_STOPPED) {
            audio_manager_stop();
        }
        lv_free(data);
    }
}

static const char* get_filename_from_path(const char* path) {
    const char* filename = strrchr(path, '/');
    return filename ? filename + 1 : path;
}