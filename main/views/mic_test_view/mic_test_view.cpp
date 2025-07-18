#include "mic_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/audio_recorder/audio_recorder.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

static const char *TAG = "MIC_TEST_VIEW";

// UI Widgets
static lv_obj_t* status_label;
static lv_obj_t* time_label;
static lv_obj_t* icon_label;
static lv_timer_t* ui_update_timer = NULL;

// State
static char current_filepath[256];

// Prototypes
static void ui_update_timer_cb(lv_timer_t* timer);
static void view_cleanup_event_cb(lv_event_t* e);

static void format_time(char* buf, size_t buf_size, uint32_t time_s) {
    snprintf(buf, buf_size, "%02lu:%02lu", time_s / 60, time_s % 60);
}

static void update_ui_for_state(audio_recorder_state_t state) {
    // This check prevents updating UI elements that might already be deleted
    if (!status_label || !time_label || !icon_label) return;

    switch (state) {
        case RECORDER_STATE_IDLE:
            lv_label_set_text(status_label, "Press OK to record");
            lv_label_set_text(time_label, "00:00");
            lv_label_set_text(icon_label, LV_SYMBOL_AUDIO);
            lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
            break;
        case RECORDER_STATE_RECORDING:
            lv_label_set_text(status_label, "Recording...");
            lv_label_set_text(icon_label, LV_SYMBOL_STOP);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);
            break;
        case RECORDER_STATE_SAVING:
            lv_label_set_text(status_label, "Saving...");
            lv_label_set_text(icon_label, LV_SYMBOL_SAVE);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
            break;
        case RECORDER_STATE_CANCELLING:
            lv_label_set_text(status_label, "Cancelling...");
            lv_label_set_text(icon_label, LV_SYMBOL_TRASH);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_GREY), 0);
            break;
        case RECORDER_STATE_ERROR:
            lv_label_set_text(status_label, "Error! Check SD card.");
            lv_label_set_text(icon_label, LV_SYMBOL_WARNING);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);
            break;
    }
}

static void ui_update_timer_cb(lv_timer_t* timer) {
    static audio_recorder_state_t last_state = (audio_recorder_state_t)-1; // Force update on first run
    audio_recorder_state_t current_state = audio_recorder_get_state();

    if (current_state != last_state) {
        ESP_LOGD(TAG, "Recorder state changed from %d to %d", last_state, current_state);
        update_ui_for_state(current_state);
        last_state = current_state;
    }

    if (current_state == RECORDER_STATE_RECORDING) {
        char time_buf[16];
        format_time(time_buf, sizeof(time_buf), audio_recorder_get_duration_s());
        lv_label_set_text(time_label, time_buf);
    }
}

static void handle_ok_press(void* user_data) {
    audio_recorder_state_t state = audio_recorder_get_state();

    if (state == RECORDER_STATE_IDLE || state == RECORDER_STATE_ERROR) {
        // --- Start Recording ---
        if (!sd_manager_check_ready()) {
            ESP_LOGE(TAG, "SD card not ready. Aborting recording.");
            update_ui_for_state(RECORDER_STATE_ERROR);
            return;
        }

        const char* mount_point = sd_manager_get_mount_point();
        char rec_dir[128];
        snprintf(rec_dir, sizeof(rec_dir), "%s/recordings", mount_point);
        
        // --- FIX: Use the sd_card_manager abstraction instead of POSIX mkdir ---
        // This ensures all SD card operations are handled consistently by the manager.
        struct stat st;
        if (stat(rec_dir, &st) == -1) {
            ESP_LOGI(TAG, "Directory '%s' not found. Creating...", rec_dir);
            if (!sd_manager_create_directory(rec_dir)) {
                ESP_LOGE(TAG, "Failed to create directory '%s' using the manager.", rec_dir);
                update_ui_for_state(RECORDER_STATE_ERROR);
                return;
            }
            ESP_LOGI(TAG, "Directory created successfully via manager.");
        }
        // --- END OF FIX ---

        time_t now = time(NULL);
        struct tm* timeinfo = localtime(&now);
        char filename[64];
        strftime(filename, sizeof(filename), "rec_%Y%m%d_%H%M%S.wav", timeinfo);
        snprintf(current_filepath, sizeof(current_filepath), "%s/%s", rec_dir, filename);

        ESP_LOGI(TAG, "Starting recording to file: %s", current_filepath);
        if (!audio_recorder_start(current_filepath)) {
            ESP_LOGE(TAG, "Failed to start audio recorder.");
            update_ui_for_state(RECORDER_STATE_ERROR);
        }
    } else if (state == RECORDER_STATE_RECORDING) {
        // --- Stop Recording ---
        ESP_LOGI(TAG, "Stopping recording.");
        audio_recorder_stop();
    }
}

static void handle_cancel_press(void* user_data) {
    // The cleanup logic is now handled by the LV_EVENT_DELETE callback.
    // We just need to load the next view.
    ESP_LOGI(TAG, "Cancel pressed. Returning to menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

/**
 * @brief This callback is triggered when the view's main container is deleted.
 * It's the ideal place to clean up all resources associated with the view.
 */
static void view_cleanup_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Mic Test View is being deleted, cleaning up resources...");

        // Ensure the recorder is stopped to prevent a background task from running wild.
        audio_recorder_state_t state = audio_recorder_get_state();
        if (state == RECORDER_STATE_RECORDING || state == RECORDER_STATE_SAVING) {
            ESP_LOGW(TAG, "View closed while recording was active. Cancelling recording.");
            audio_recorder_cancel();
        }

        // Delete the timer associated with this view to prevent memory leaks.
        if (ui_update_timer) {
            lv_timer_delete(ui_update_timer);
            ui_update_timer = NULL;
        }

        // Nullify pointers to UI elements to prevent dangling references.
        status_label = NULL;
        time_label = NULL;
        icon_label = NULL;
    }
}

void mic_test_view_create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Mic Test View");

    // Main container
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Add the cleanup callback to the main container. This is the key for robust resource management.
    lv_obj_add_event_cb(cont, view_cleanup_event_cb, LV_EVENT_DELETE, NULL);

    // Title
    lv_obj_t* title_label = lv_label_create(cont);
    lv_label_set_text(title_label, "Microphone Test");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);

    // Icon (Microphone/Stop/Save)
    icon_label = lv_label_create(cont);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);

    // Time Label
    time_label = lv_label_create(cont);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_28, 0);

    // Status Label
    status_label = lv_label_create(cont);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);

    // Set initial state
    update_ui_for_state(audio_recorder_get_state());

    // Create a timer to periodically update the UI
    ui_update_timer = lv_timer_create(ui_update_timer_cb, 250, NULL);

    // Register button handlers
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
}