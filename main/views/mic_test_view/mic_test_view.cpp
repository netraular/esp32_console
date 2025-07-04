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
static lv_timer_t* ui_update_timer;

// State
static char current_filepath[256];

// Prototypes
static void ui_update_timer_cb(lv_timer_t* timer);
static void cleanup_mic_test_view();

static void format_time(char* buf, size_t buf_size, uint32_t time_s) {
    snprintf(buf, buf_size, "%02lu:%02lu", time_s / 60, time_s % 60);
}

static void update_ui_for_state(audio_recorder_state_t state) {
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
        ESP_LOGI(TAG, "Recorder state changed from %d to %d", last_state, current_state);
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
        // 1. Check if SD card is ready
        if (!sd_manager_check_ready()) {
            ESP_LOGE(TAG, "SD card not ready. Aborting recording.");
            update_ui_for_state(RECORDER_STATE_ERROR);
            return;
        }

        const char* mount_point = sd_manager_get_mount_point();
        char rec_dir[128];
        snprintf(rec_dir, sizeof(rec_dir), "%s/recordings", mount_point);
        
        // 2. Create recordings directory if it doesn't exist
        struct stat st;
        if (stat(rec_dir, &st) == -1) {
            ESP_LOGI(TAG, "Directory '%s' not found. Attempting to create...", rec_dir);
            if (mkdir(rec_dir, 0755) != 0) {
                ESP_LOGE(TAG, "Failed to create directory '%s'. Error: %s", rec_dir, strerror(errno));
                update_ui_for_state(RECORDER_STATE_ERROR);
                return;
            }
            ESP_LOGI(TAG, "Directory created successfully.");
        }

        // 3. Generate filename based on timestamp
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
    cleanup_mic_test_view();
    view_manager_load_view(VIEW_ID_MENU);
}

static void cleanup_mic_test_view() {
    if (audio_recorder_get_state() == RECORDER_STATE_RECORDING) {
        audio_recorder_stop();
    }
    if (ui_update_timer) {
        lv_timer_delete(ui_update_timer);
        ui_update_timer = NULL;
    }
    ESP_LOGI(TAG, "Mic test view cleaned up.");
}

void mic_test_view_create(lv_obj_t* parent) {
    // Main container
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

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

    // Register button handlers using the new API for single click events.
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_SINGLE_CLICK, handle_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_SINGLE_CLICK, handle_cancel_press, true, nullptr);
}