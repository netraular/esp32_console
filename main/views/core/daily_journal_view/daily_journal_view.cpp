#include "daily_journal_view.h"
#include "views/view_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/daily_summary_manager/daily_summary_manager.h"
#include "models/asset_config.h"
#include "esp_log.h"
#include <time.h>
#include <sys/stat.h>
#include <cstring>

static const char *TAG = "DAILY_JOURNAL_VIEW";

// --- Lifecycle Methods ---

DailyJournalView::DailyJournalView() {
    ESP_LOGI(TAG, "Constructed");
    last_known_state = (audio_recorder_state_t)-1; // Force initial UI update
}

DailyJournalView::~DailyJournalView() {
    ESP_LOGI(TAG, "Destructed, cleaning up resources.");
    if (ui_update_timer) {
        lv_timer_del(ui_update_timer);
        ui_update_timer = nullptr;
    }
    if (audio_recorder_get_state() == RECORDER_STATE_RECORDING) {
        ESP_LOGW(TAG, "View deleted during recording. Cancelling operation.");
        audio_recorder_cancel();
    }
}

void DailyJournalView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    
    setup_ui(container);
    setup_button_handlers();

    ui_update_timer = lv_timer_create(DailyJournalView::ui_update_timer_cb, 250, this);
}

// --- UI & Handler Setup ---

void DailyJournalView::setup_ui(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Daily Journal");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);

    icon_label = lv_label_create(parent);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);

    time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_28, 0);

    status_label = lv_label_create(parent);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);

    update_ui_for_state(audio_recorder_get_state());
}

void DailyJournalView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, DailyJournalView::ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, DailyJournalView::cancel_press_cb, true, this);
}

// --- UI Logic & Helpers ---

void DailyJournalView::format_time(char* buf, size_t buf_size, uint32_t time_s) {
    snprintf(buf, buf_size, "%02lu:%02lu", time_s / 60, time_s % 60);
}

void DailyJournalView::update_ui_for_state(audio_recorder_state_t state) {
    switch (state) {
        case RECORDER_STATE_IDLE:
            lv_label_set_text(status_label, "Press OK to record today's entry");
            lv_label_set_text(time_label, "00:00");
            lv_label_set_text(icon_label, LV_SYMBOL_AUDIO);
            lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
            break;
        case RECORDER_STATE_RECORDING:
            lv_label_set_text(status_label, "Recording journal...");
            lv_label_set_text(icon_label, LV_SYMBOL_STOP);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);
            break;
        case RECORDER_STATE_SAVING:
            lv_label_set_text(status_label, "Saving entry...");
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

void DailyJournalView::update_ui() {
    audio_recorder_state_t current_state = audio_recorder_get_state();
    if (current_state != last_known_state) {
        ESP_LOGD(TAG, "Recorder state changed from %d to %d", last_known_state, current_state);

        // Check for successful save completion
        if (last_known_state == RECORDER_STATE_SAVING && current_state == RECORDER_STATE_IDLE) {
            ESP_LOGI(TAG, "Journal entry saved successfully. Updating daily summary with path: %s", current_filepath.c_str());
            // BUG FIX: The path stored in current_filepath is the correct, full path.
            // Do not prepend SD_CARD_ROOT_PATH again.
            DailySummaryManager::set_journal_path(time(NULL), current_filepath);
        }

        update_ui_for_state(current_state);
        last_known_state = current_state;
    }
    if (current_state == RECORDER_STATE_RECORDING) {
        char time_buf[16];
        format_time(time_buf, sizeof(time_buf), audio_recorder_get_duration_s());
        lv_label_set_text(time_label, time_buf);
    }
}

void DailyJournalView::get_todays_filepath(std::string& path_from_root) {
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char filename_buf[32];
    strftime(filename_buf, sizeof(filename_buf), "journal_%Y%m%d.wav", &timeinfo);
    
    // Path relative to VFS root, e.g. /sdcard/userdata/journal/journal_...
    path_from_root = std::string(SD_CARD_ROOT_PATH) + "/" + USER_DATA_BASE_PATH + JOURNAL_SUBPATH + filename_buf;
}

void DailyJournalView::start_recording() {
    ESP_LOGI(TAG, "Starting new journal entry: %s", current_filepath.c_str());
    if (!audio_recorder_start(current_filepath.c_str())) {
        update_ui_for_state(RECORDER_STATE_ERROR);
    }
}

// --- Instance Methods for Actions ---

void DailyJournalView::on_ok_press() {
    audio_recorder_state_t state = audio_recorder_get_state();

    if (state == RECORDER_STATE_IDLE || state == RECORDER_STATE_ERROR) {
        if (!sd_manager_check_ready()) {
            ESP_LOGE(TAG, "SD card not ready. Aborting recording.");
            update_ui_for_state(RECORDER_STATE_ERROR);
            return;
        }

        get_todays_filepath(current_filepath);
        ESP_LOGI(TAG, "Checking for existing journal entry at %s", current_filepath.c_str());

        if (sd_manager_file_exists(current_filepath.c_str())) {
            ESP_LOGI(TAG, "Existing entry found. Prompting user for overwrite.");
            popup_manager_show_confirmation(
                "Overwrite?",
                "An entry for today already exists. Do you want to replace it?",
                "Replace",
                "Cancel",
                DailyJournalView::overwrite_popup_cb,
                this
            );
        } else {
            // No existing file, proceed directly to recording
            std::string journal_dir = std::string(SD_CARD_ROOT_PATH) + "/" + USER_DATA_BASE_PATH + JOURNAL_SUBPATH;
            if (!sd_manager_create_directory(journal_dir.c_str())) {
                ESP_LOGE(TAG, "Failed to create journal directory: %s", journal_dir.c_str());
                update_ui_for_state(RECORDER_STATE_ERROR);
                return;
            }
            start_recording();
        }
    } else if (state == RECORDER_STATE_RECORDING) {
        ESP_LOGI(TAG, "Stopping journal recording and saving file.");
        audio_recorder_stop();
    }
}

void DailyJournalView::on_cancel_press() {
    audio_recorder_state_t state = audio_recorder_get_state();
    if (state == RECORDER_STATE_RECORDING) {
        ESP_LOGI(TAG, "Cancel pressed during recording. Discarding file.");
        audio_recorder_cancel();
    } else if (!popup_manager_is_active()) {
        ESP_LOGI(TAG, "Cancel pressed. Returning to menu.");
        view_manager_load_view(VIEW_ID_MENU);
    }
}

void DailyJournalView::handle_overwrite_confirmation(popup_result_t result) {
    if (result == POPUP_RESULT_PRIMARY) {
        ESP_LOGI(TAG, "User chose to overwrite. Deleting old file.");
        if (!sd_manager_delete_item(current_filepath.c_str())) {
            ESP_LOGE(TAG, "Failed to delete existing journal file. Aborting.");
            update_ui_for_state(RECORDER_STATE_ERROR);
        } else {
            start_recording();
        }
    } else {
        ESP_LOGI(TAG, "User cancelled overwrite.");
    }
    // IMPORTANT: Re-enable our view's input handlers after the popup is closed.
    setup_button_handlers();
}

// --- Static Callbacks ---

void DailyJournalView::ok_press_cb(void* user_data) {
    static_cast<DailyJournalView*>(user_data)->on_ok_press();
}

void DailyJournalView::cancel_press_cb(void* user_data) {
    static_cast<DailyJournalView*>(user_data)->on_cancel_press();
}

void DailyJournalView::ui_update_timer_cb(lv_timer_t* timer) {
    auto* view = static_cast<DailyJournalView*>(lv_timer_get_user_data(timer));
    if (view) {
        view->update_ui();
    }
}

void DailyJournalView::overwrite_popup_cb(popup_result_t result, void* user_data) {
    auto* view = static_cast<DailyJournalView*>(user_data);
    if (view) {
        view->handle_overwrite_confirmation(result);
    }
}