#include "voice_note_view.h"
#include "views/view_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include <time.h>
#include <sys/stat.h>
#include <cstring>

static const char *TAG = "VOICE_NOTE_VIEW";

// --- Lifecycle Methods ---

VoiceNoteView::VoiceNoteView() {
    ESP_LOGI(TAG, "VoiceNoteView constructed");
    last_known_state = (audio_recorder_state_t)-1; // Force initial UI update
}

VoiceNoteView::~VoiceNoteView() {
    ESP_LOGI(TAG, "VoiceNoteView destructed, cleaning up resources.");

    if (ui_update_timer) {
        lv_timer_del(ui_update_timer);
        ui_update_timer = nullptr;
    }

    // If the view is destroyed mid-recording, cancel it to avoid an orphaned task/file.
    if (audio_recorder_get_state() == RECORDER_STATE_RECORDING) {
        ESP_LOGW(TAG, "View deleted during recording. Cancelling operation.");
        audio_recorder_cancel();
    }
}

void VoiceNoteView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Voice Note View UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    
    setup_ui(container);
    setup_button_handlers();

    ui_update_timer = lv_timer_create(VoiceNoteView::ui_update_timer_cb, 250, this);
}

// --- UI & Handler Setup ---

void VoiceNoteView::setup_ui(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Voice Notes");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);

    icon_label = lv_label_create(parent);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);

    time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_28, 0);

    status_label = lv_label_create(parent);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);

    update_ui_for_state(audio_recorder_get_state());
}

void VoiceNoteView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, VoiceNoteView::ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, VoiceNoteView::right_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, VoiceNoteView::cancel_press_cb, true, this);
}

// --- UI Logic ---

void VoiceNoteView::format_time(char* buf, size_t buf_size, uint32_t time_s) {
    snprintf(buf, buf_size, "%02lu:%02lu", time_s / 60, time_s % 60);
}

void VoiceNoteView::update_ui_for_state(audio_recorder_state_t state) {
    switch (state) {
        case RECORDER_STATE_IDLE:
            lv_label_set_text(status_label, "OK: Record | Right: Play Notes");
            lv_label_set_text(time_label, "00:00");
            lv_label_set_text(icon_label, LV_SYMBOL_AUDIO); // Microphone icon
            lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
            break;
        case RECORDER_STATE_RECORDING:
            lv_label_set_text(status_label, "Recording note...");
            lv_label_set_text(icon_label, LV_SYMBOL_STOP);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);
            break;
        case RECORDER_STATE_SAVING:
            lv_label_set_text(status_label, "Saving note...");
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

void VoiceNoteView::update_ui() {
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

void VoiceNoteView::on_ok_press() {
    audio_recorder_state_t state = audio_recorder_get_state();

    if (state == RECORDER_STATE_IDLE || state == RECORDER_STATE_ERROR) {
        if (!sd_manager_check_ready()) {
            ESP_LOGE(TAG, "SD card not ready. Aborting recording.");
            update_ui_for_state(RECORDER_STATE_ERROR);
            return;
        }

        const char* notes_dir = "/sdcard/notes";
        struct stat st;
        if (stat(notes_dir, &st) == -1) {
            ESP_LOGI(TAG, "Directory '%s' not found. Creating...", notes_dir);
            if (!sd_manager_create_directory(notes_dir)) {
                update_ui_for_state(RECORDER_STATE_ERROR);
                return;
            }
        }
        
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char filename[64];
        strftime(filename, sizeof(filename), "note_%Y%m%d_%H%M%S.wav", &timeinfo);
        snprintf(current_filepath, sizeof(current_filepath), "%s/%s", notes_dir, filename);

        ESP_LOGI(TAG, "Starting new voice note: %s", current_filepath);
        if (!audio_recorder_start(current_filepath)) {
            update_ui_for_state(RECORDER_STATE_ERROR);
        }
    } else if (state == RECORDER_STATE_RECORDING) {
        ESP_LOGI(TAG, "Stopping voice note recording and saving file.");
        audio_recorder_stop();
    }
}

void VoiceNoteView::on_right_press() {
    audio_recorder_state_t state = audio_recorder_get_state();
    if (state == RECORDER_STATE_IDLE || state == RECORDER_STATE_ERROR) {
        ESP_LOGI(TAG, "Right press detected, loading voice note player.");
        view_manager_load_view(VIEW_ID_VOICE_NOTE_PLAYER);
    }
}

void VoiceNoteView::on_cancel_press() {
    audio_recorder_state_t state = audio_recorder_get_state();
    if (state == RECORDER_STATE_RECORDING) {
        ESP_LOGI(TAG, "Cancel pressed during recording. Discarding file.");
        audio_recorder_cancel();
    } else {
        ESP_LOGI(TAG, "Cancel pressed. Returning to menu.");
        view_manager_load_view(VIEW_ID_MENU);
    }
}

// --- Static Callbacks (Bridge to C-style APIs) ---

void VoiceNoteView::ok_press_cb(void* user_data) {
    static_cast<VoiceNoteView*>(user_data)->on_ok_press();
}

void VoiceNoteView::right_press_cb(void* user_data) {
    static_cast<VoiceNoteView*>(user_data)->on_right_press();
}

void VoiceNoteView::cancel_press_cb(void* user_data) {
    static_cast<VoiceNoteView*>(user_data)->on_cancel_press();
}

void VoiceNoteView::ui_update_timer_cb(lv_timer_t* timer) {
    auto* view = static_cast<VoiceNoteView*>(lv_timer_get_user_data(timer));
    if (view) {
        view->update_ui();
    }
}