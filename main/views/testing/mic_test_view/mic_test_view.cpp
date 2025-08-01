#include "mic_test_view.h"
#include "views/view_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"
#include <time.h>
#include <sys/stat.h>
#include <cstring>

static const char *TAG = "MIC_TEST_VIEW";

// --- Lifecycle Methods ---
MicTestView::MicTestView() {
    ESP_LOGI(TAG, "MicTestView constructed");
    // Initialize non-zero/nullptr members if needed
    last_known_state = (audio_recorder_state_t)-1; // Force initial UI update
}

MicTestView::~MicTestView() {
    ESP_LOGI(TAG, "MicTestView destructed, cleaning up resources...");

    // Ensure the recorder is stopped cleanly to prevent a background task from running wild.
    audio_recorder_state_t state = audio_recorder_get_state();
    if (state == RECORDER_STATE_RECORDING || state == RECORDER_STATE_SAVING) {
        ESP_LOGW(TAG, "View closed while recording was active. Cancelling recording.");
        audio_recorder_cancel();
    }

    // Delete the timer associated with this view to prevent memory leaks.
    if (ui_update_timer) {
        lv_timer_del(ui_update_timer);
        ui_update_timer = nullptr;
    }
    // LVGL widgets (status_label, etc.) are children of 'container' and will be
    // automatically deleted by the view_manager. No need to delete them here.
}

void MicTestView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Mic Test View UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    
    setup_ui(container);
    setup_button_handlers();

    // Create a timer to periodically update the UI
    ui_update_timer = lv_timer_create(MicTestView::ui_update_timer_cb, 250, this);
}

// --- UI & Handler Setup ---
void MicTestView::setup_ui(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Title
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Microphone Test");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);

    // Icon (Microphone/Stop/Save)
    icon_label = lv_label_create(parent);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);

    // Time Label
    time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_28, 0);

    // Status Label
    status_label = lv_label_create(parent);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);

    // Set initial state
    update_ui_for_state(audio_recorder_get_state());
}

void MicTestView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, MicTestView::ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, MicTestView::cancel_press_cb, true, this);
}

// --- UI Logic ---
void MicTestView::format_time(char* buf, size_t buf_size, uint32_t time_s) {
    snprintf(buf, buf_size, "%02lu:%02lu", time_s / 60, time_s % 60);
}

void MicTestView::update_ui_for_state(audio_recorder_state_t state) {
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

void MicTestView::update_ui() {
    audio_recorder_state_t current_state = audio_recorder_get_state();

    if (current_state != last_known_state) {
        ESP_LOGD(TAG, "Recorder state changed from %d to %d", last_known_state, current_state);
        update_ui_for_state(current_state);
        last_known_state = current_state;
    }

    if (current_state == RECORDER_STATE_RECORDING) {
        char time_buf[16];
        format_time(time_buf, sizeof(time_buf), audio_recorder_get_duration_s());
        lv_label_set_text(time_label, time_buf);
    }
}

// --- Instance Methods for Actions ---
void MicTestView::on_ok_press() {
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
        
        struct stat st;
        if (stat(rec_dir, &st) == -1) {
            ESP_LOGI(TAG, "Directory '%s' not found. Creating...", rec_dir);
            if (!sd_manager_create_directory(rec_dir)) {
                ESP_LOGE(TAG, "Failed to create directory '%s'", rec_dir);
                update_ui_for_state(RECORDER_STATE_ERROR);
                return;
            }
        }
        
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char filename[64];
        strftime(filename, sizeof(filename), "rec_%Y%m%d_%H%M%S.wav", &timeinfo);
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

void MicTestView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed. Returning to menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks (Bridge to C-style APIs) ---
void MicTestView::ok_press_cb(void* user_data) {
    static_cast<MicTestView*>(user_data)->on_ok_press();
}

void MicTestView::cancel_press_cb(void* user_data) {
    static_cast<MicTestView*>(user_data)->on_cancel_press();
}

void MicTestView::ui_update_timer_cb(lv_timer_t* timer) {
    MicTestView* view = static_cast<MicTestView*>(lv_timer_get_user_data(timer));
    if (view) {
        view->update_ui();
    }
}