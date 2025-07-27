#include "speaker_test_view.h"
#include "views/view_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "components/audio_player_component/audio_player_component.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "SPEAKER_TEST_VIEW";

// --- Lifecycle Methods ---
SpeakerTestView::SpeakerTestView() {
    ESP_LOGI(TAG, "SpeakerTestView constructed");
}

SpeakerTestView::~SpeakerTestView() {
    ESP_LOGI(TAG, "SpeakerTestView destructed");
}

void SpeakerTestView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Speaker Test View");
    container = parent;
    create_initial_view();
}

// --- UI & Handler Setup ---
void SpeakerTestView::setup_initial_button_handlers() {
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, SpeakerTestView::initial_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, SpeakerTestView::initial_cancel_press_cb, true, this);
}

void SpeakerTestView::create_initial_view() {
    lv_obj_clean(container);

    lv_obj_t* title_label = lv_label_create(container);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(title_label, "Speaker Test");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    info_label = lv_label_create(container);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label);
    lv_label_set_text(info_label, "Press OK to select\na .wav audio file.");

    setup_initial_button_handlers();
}

void SpeakerTestView::show_file_explorer() {
    lv_obj_clean(container);

    file_explorer_host_container = lv_obj_create(container);
    lv_obj_remove_style_all(file_explorer_host_container);
    lv_obj_set_size(file_explorer_host_container, LV_PCT(100), LV_PCT(100));

    lv_obj_add_event_cb(file_explorer_host_container, explorer_cleanup_event_cb, LV_EVENT_DELETE, this);

    // --- MODIFIED --- Pass `this` as user_data.
    file_explorer_create(
        file_explorer_host_container,
        sd_manager_get_mount_point(),
        audio_file_selected_cb_c,
        nullptr, 
        nullptr, 
        explorer_exit_cb_c,
        this
    );
}

// --- Instance Methods for Actions ---
void SpeakerTestView::on_initial_ok_press() {
    if (sd_manager_check_ready()) {
        show_file_explorer();
    } else if (info_label) {
        lv_label_set_text(info_label, "Failed to read SD card.\nCheck card and press OK to retry.");
    }
}

void SpeakerTestView::on_initial_cancel_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

void SpeakerTestView::on_audio_file_selected(const char* path) {
    const char* ext = strrchr(path, '.');
    if (ext && (strcasecmp(ext, ".wav") == 0)) {
        ESP_LOGI(TAG, "WAV file selected: %s. Starting player.", path);
        
        lv_obj_clean(container);

        // --- MODIFIED --- Pass `this` as user_data.
        audio_player_component_create(container, path, player_exit_cb_c, this);
    } else {
        ESP_LOGW(TAG, "File selected is not a .wav file: %s", path);
    }
}

void SpeakerTestView::on_explorer_exit_from_root() {
    ESP_LOGI(TAG, "Exited file explorer from root. Returning to initial view.");
    create_initial_view();
}

void SpeakerTestView::on_player_exit() {
    ESP_LOGI(TAG, "Exiting audio player, returning to initial speaker test view.");
    create_initial_view();
}

// --- Static Callbacks for Button Manager ---
void SpeakerTestView::initial_ok_press_cb(void* user_data) {
    static_cast<SpeakerTestView*>(user_data)->on_initial_ok_press();
}

void SpeakerTestView::initial_cancel_press_cb(void* user_data) {
    static_cast<SpeakerTestView*>(user_data)->on_initial_cancel_press();
}

// --- Static Callbacks for C Components ---
void SpeakerTestView::audio_file_selected_cb_c(const char* path, void* user_data) {
    if(user_data) static_cast<SpeakerTestView*>(user_data)->on_audio_file_selected(path);
}

void SpeakerTestView::explorer_exit_cb_c(void* user_data) {
    if(user_data) static_cast<SpeakerTestView*>(user_data)->on_explorer_exit_from_root();
}

void SpeakerTestView::player_exit_cb_c(void* user_data) {
    if(user_data) static_cast<SpeakerTestView*>(user_data)->on_player_exit();
}

void SpeakerTestView::explorer_cleanup_event_cb(lv_event_t * e) {
    ESP_LOGD(TAG, "Explorer host container deleted. Calling file_explorer_destroy().");
    file_explorer_destroy();
    auto* instance = static_cast<SpeakerTestView*>(lv_event_get_user_data(e));
    if (instance) {
        instance->file_explorer_host_container = nullptr;
    }
}