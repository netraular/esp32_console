#include "speaker_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "../file_explorer/file_explorer.h"
#include "components/audio_visualizer/audio_visualizer.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "SPEAKER_TEST_VIEW";

// View State Variables
static lv_obj_t *view_parent = NULL;
static char current_song_path[256];
static bool is_exiting = false; // Flag to manage a clean shutdown sequence.

// UI Widgets
static lv_obj_t *play_pause_btn_label = NULL;
static lv_obj_t *slider_widget = NULL;
static lv_obj_t *time_current_label_widget = NULL;
static lv_obj_t *time_total_label_widget = NULL;
static lv_timer_t *ui_update_timer = NULL;
static lv_obj_t *volume_label_widget = NULL;
static lv_obj_t *visualizer_widget = NULL;
static lv_obj_t *info_label_widget = NULL; // For initial/error screens.
static visualizer_data_t current_viz_data;
static bool viz_data_received = false; // Used for visualizer decay effect.

// Worker Function Prototypes (for lv_async_call)
static void update_volume_label_task(void *user_data);
static void handle_play_pause_ui_update_task(void *user_data);
static void stop_audio_task(void* user_data);

// Other Function Prototypes
static void create_initial_speaker_view();
static void show_file_explorer();
static void create_now_playing_view(const char *file_path);
static void cleanup_player_and_return_to_initial_view();
static void ui_update_timer_cb(lv_timer_t *timer);

// Callbacks and button handlers
static void on_audio_file_selected(const char *path);
static void on_explorer_exit_speaker();
static void handle_ok_press_initial();
static void handle_cancel_press_initial();
static void handle_ok_press_playing();
static void handle_cancel_press_playing();
static void handle_left_press_playing();
static void handle_right_press_playing();


// --- Utility Functions ---
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

// This timer is the heart of the "now playing" screen. It polls for state changes
// and updates the UI accordingly.
static void ui_update_timer_cb(lv_timer_t *timer) {
    audio_player_state_t state = audio_manager_get_state();
    
    // If a shutdown was requested and the audio manager confirms it's stopped, clean up the UI.
    if (is_exiting && state == AUDIO_STATE_STOPPED) {
        ESP_LOGI(TAG, "Audio manager confirmed STOPPED state. Cleaning up UI.");
        cleanup_player_and_return_to_initial_view();
        return; // Stop further processing in this tick.
    }
    
    // If an audio error is detected (e.g., SD card removed) and we are not already exiting...
    if (state == AUDIO_STATE_ERROR && !is_exiting) {
        ESP_LOGW(TAG, "Audio manager reported an error. Initiating safe shutdown.");
        is_exiting = true;                          // 1. Mark that we are in the exit process.
        button_manager_unregister_view_handlers();  // 2. Disable buttons to prevent further interaction.
        sd_manager_unmount();                       // 3. Unmount the SD card immediately.
        lv_async_call(stop_audio_task, NULL);       // 4. Safely request the audio manager to stop.
        
        // The rest of the UI cleanup will happen on the next timer tick when the
        // `is_exiting && state == AUDIO_STATE_STOPPED` condition is met.
        return;
    }

    // If the song finishes naturally (not due to an error or user action).
    if (!is_exiting && state == AUDIO_STATE_STOPPED) {
        ESP_LOGI(TAG, "Song finished naturally. Resetting UI to initial playback state.");
        handle_play_pause_ui_update_task(NULL);
        if (slider_widget) lv_slider_set_value(slider_widget, 0, LV_ANIM_OFF);
        if (time_current_label_widget) lv_label_set_text(time_current_label_widget, "00:00");
        
        // Reset visualizer to zero.
        if (visualizer_widget) {
            uint8_t zero_values[VISUALIZER_BAR_COUNT] = {0};
            audio_visualizer_set_values(visualizer_widget, zero_values);
        }
        
        // Stop the timer to save CPU. It will be restarted on the next "play" action.
        if (ui_update_timer) {
            lv_timer_delete(ui_update_timer);
            ui_update_timer = NULL;
        }
        return;
    }

    // Regular UI updates during playback
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

    // Visualizer Update Logic
    QueueHandle_t queue = audio_manager_get_visualizer_queue();
    if (queue && visualizer_widget) {
        // Check for new data from the audio manager.
        if (xQueueReceive(queue, &current_viz_data, 0) == pdPASS) {
            viz_data_received = true;
            audio_visualizer_set_values(visualizer_widget, current_viz_data.bar_values);
        } else {
            // If no new data, create a "decay" effect by slowly reducing bar heights.
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
                    viz_data_received = false; // All bars are zero, stop decaying.
                }
            }
        }
    }
}


// --- "Worker" Functions for lv_async_call ---
// These functions are called asynchronously to avoid blocking the caller (e.g., button handlers).

// Updates the volume label on the screen.
static void update_volume_label_task(void *user_data) {
    if (volume_label_widget) {
        char vol_buf[16];
        uint8_t vol = audio_manager_get_volume();
        // Map physical volume (e.g. 0-25) to display volume (0-100) and snap to steps.
        uint8_t raw_display_vol = (vol * 100) / MAX_VOLUME_PERCENTAGE;
        const int volume_step = 5;
        uint8_t display_vol = (uint8_t)(roundf((float)raw_display_vol / volume_step) * volume_step);
        
        const char * vol_icon = (display_vol == 0) ? LV_SYMBOL_MUTE : (vol < (MAX_VOLUME_PERCENTAGE / 2)) ? LV_SYMBOL_VOLUME_MID : LV_SYMBOL_VOLUME_MAX;
        snprintf(vol_buf, sizeof(vol_buf), "%s %u%%", vol_icon, display_vol);
        lv_label_set_text(volume_label_widget, vol_buf);
    }
}

// Updates the play/pause button icon based on the audio manager's state.
static void handle_play_pause_ui_update_task(void *user_data) {
    audio_player_state_t state = audio_manager_get_state();
    if (play_pause_btn_label) {
        lv_label_set_text(play_pause_btn_label, (state == AUDIO_STATE_PLAYING) ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }
}

// Safely calls the blocking audio_manager_stop() function from a separate task context.
static void stop_audio_task(void* user_data) {
    audio_manager_stop();
}

// --- Button Handlers for "Now Playing" View ---

static void handle_ok_press_playing() {
    audio_player_state_t state = audio_manager_get_state();
    switch (state) {
        case AUDIO_STATE_STOPPED:
            if (is_exiting) return; // Prevent starting a new song while exiting.
            if (audio_manager_play(current_song_path)) {
                lv_async_call(handle_play_pause_ui_update_task, NULL);
                if (ui_update_timer == NULL) { // Restart the timer if it was stopped.
                    ui_update_timer = lv_timer_create(ui_update_timer_cb, 50, NULL);
                }
            } else {
                ESP_LOGE(TAG, "Failed to start audio playback. Returning to initial view.");
                cleanup_player_and_return_to_initial_view();
            }
            break;
        case AUDIO_STATE_PLAYING:
            audio_manager_pause();
            lv_async_call(handle_play_pause_ui_update_task, NULL);
            break;
        case AUDIO_STATE_PAUSED:
            audio_manager_resume();
            lv_async_call(handle_play_pause_ui_update_task, NULL);
            break;
        default: break;
    }
}

static void handle_cancel_press_playing() {
    if (is_exiting) return; // Prevent multiple exit requests.

    ESP_LOGI(TAG, "Cancel pressed. Requesting audio stop asynchronously.");
    is_exiting = true;
    button_manager_unregister_view_handlers(); // Disable buttons immediately.
    
    // Call the stop function asynchronously to avoid blocking the button handler.
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


// Cleans up all resources of the "now playing" view and returns to the initial speaker test screen.
static void cleanup_player_and_return_to_initial_view() {
    if (ui_update_timer) {
        lv_timer_delete(ui_update_timer);
        ui_update_timer = NULL;
    }
    
    // Nullify all widget pointers to prevent use-after-free.
    play_pause_btn_label = NULL;
    volume_label_widget = NULL;
    visualizer_widget = NULL;
    slider_widget = NULL;
    time_current_label_widget = NULL;
    time_total_label_widget = NULL;
    
    create_initial_speaker_view();
}

// Callback for the file explorer: if a .wav file is selected, create the "now playing" view.
static void on_audio_file_selected(const char *path) {
    if (is_wav_file(path)) {
        file_explorer_destroy();
        create_now_playing_view(path);
    }
}

// Callback for the file explorer: if the user exits, return to the initial speaker test screen.
static void on_explorer_exit_speaker() {
    file_explorer_destroy(); 
    create_initial_speaker_view(); 
}

// Hides the initial view and shows the file explorer.
static void show_file_explorer() {
    lv_obj_clean(view_parent);
    lv_obj_t * explorer_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(100), lv_pct(100));
    lv_obj_center(explorer_container);
    file_explorer_create(explorer_container, sd_manager_get_mount_point(), on_audio_file_selected, on_explorer_exit_speaker);
}

// Creates the main "now playing" UI.
static void create_now_playing_view(const char *file_path) {
    is_exiting = false; // Reset exit flag when creating the view.
    viz_data_received = false;
    lv_obj_clean(view_parent);
    strncpy(current_song_path, file_path, sizeof(current_song_path) - 1);

    // Main container (Flexbox layout)
    lv_obj_t *main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Top container for title and volume
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
    update_volume_label_task(NULL); // Set initial volume text
    lv_obj_set_style_text_font(volume_label_widget, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(volume_label_widget, LV_TEXT_ALIGN_RIGHT, 0);

    // Audio visualizer
    visualizer_widget = audio_visualizer_create(main_cont, VISUALIZER_BAR_COUNT);
    lv_obj_set_size(visualizer_widget, lv_pct(100), lv_pct(40));
    
    // Progress bar and time labels
    lv_obj_t *progress_cont = lv_obj_create(main_cont);
    lv_obj_remove_style_all(progress_cont);
    lv_obj_set_size(progress_cont, lv_pct(95), LV_SIZE_CONTENT);
    lv_obj_clear_flag(progress_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_top(progress_cont, 10, 0); 

    slider_widget = lv_slider_create(progress_cont);
    lv_obj_remove_flag(slider_widget, LV_OBJ_FLAG_CLICKABLE); // User cannot seek
    lv_obj_set_width(slider_widget, lv_pct(100));
    lv_obj_align(slider_widget, LV_ALIGN_TOP_MID, 0, 0);

    time_current_label_widget = lv_label_create(progress_cont);
    lv_label_set_text(time_current_label_widget, "00:00");
    lv_obj_align_to(time_current_label_widget, slider_widget, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    time_total_label_widget = lv_label_create(progress_cont);
    lv_label_set_text(time_total_label_widget, "??:??");
    lv_obj_align_to(time_total_label_widget, slider_widget, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 5);

    // Play/Pause button
    lv_obj_t *play_pause_btn = lv_button_create(main_cont);
    play_pause_btn_label = lv_label_create(play_pause_btn);
    lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(play_pause_btn_label, &lv_font_montserrat_28, 0);
    
    // Register button handlers for this screen
    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_playing);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_playing);
    button_manager_register_view_handler(BUTTON_LEFT, handle_left_press_playing);
    button_manager_register_view_handler(BUTTON_RIGHT, handle_right_press_playing);

    // Automatically start playing the selected file.
    handle_ok_press_playing();
}


// "OK" button on initial screen: tries to mount SD and show file explorer.
static void handle_ok_press_initial() {
    ESP_LOGI(TAG, "OK button pressed. Forcing SD card re-mount...");
    
    sd_manager_unmount();

    if (sd_manager_mount()) {
        ESP_LOGI(TAG, "Re-mount successful. Opening file explorer.");
        show_file_explorer();
    } else {
        ESP_LOGW(TAG, "Re-mount failed. Showing error message.");
        if (info_label_widget) {
            lv_label_set_text(info_label_widget, "Failed to read SD card.\n\n"
                                                 "Check the card and\n"
                                                 "press OK to retry.");
        }
    }
}

// "Cancel" button on initial screen: returns to main menu.
static void handle_cancel_press_initial() {
    if (ui_update_timer) {
        lv_timer_delete(ui_update_timer);
        ui_update_timer = NULL;
    }
    // Stop any potentially running audio just in case.
    audio_manager_stop(); 
    view_manager_load_view(VIEW_ID_MENU);
}

// Creates the very first screen for the speaker test.
static void create_initial_speaker_view() {
    lv_obj_clean(view_parent); // Ensure parent is clean.

    lv_obj_t *label = lv_label_create(view_parent);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_label_set_text(label, "Speaker Test");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    info_label_widget = lv_label_create(view_parent);
    lv_obj_set_style_text_align(info_label_widget, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label_widget);
    lv_label_set_text(info_label_widget, "Press OK to\nselect an audio file");

    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_initial);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_initial);
}

void speaker_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Speaker Test View");
    view_parent = parent;
    create_initial_speaker_view();
}