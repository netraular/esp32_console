#include "speaker_test_view.h"
#include "../view_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "components/audio_player_component/audio_player_component.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "SPEAKER_TEST_VIEW";

// --- Static Member Initialization ---
SpeakerTestView* SpeakerTestView::s_instance = nullptr;

// --- Lifecycle Methods ---
SpeakerTestView::SpeakerTestView() {
    ESP_LOGI(TAG, "SpeakerTestView constructed");
    s_instance = this; // Set the singleton instance for C callbacks
}

SpeakerTestView::~SpeakerTestView() {
    ESP_LOGI(TAG, "SpeakerTestView destructed");
    // The file_explorer_host_container is an LVGL object that will be cleaned up
    // by the ViewManager. The LV_EVENT_DELETE on it will call explorer_cleanup_event_cb,
    // which handles the C-component's destruction. No other manual cleanup is needed here.
    if (s_instance == this) {
        s_instance = nullptr; // Clear the singleton instance
    }
}

void SpeakerTestView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Speaker Test View");
    container = parent; // Use the parent directly as our container
    create_initial_view();
}

// --- UI & Handler Setup ---
void SpeakerTestView::setup_initial_button_handlers() {
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, SpeakerTestView::initial_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, SpeakerTestView::initial_cancel_press_cb, true, this);
}

void SpeakerTestView::create_initial_view() {
    // This is the entry point for the view and also the return point from other states.
    // Cleaning the parent object ensures that any previous UI (explorer, player) is removed.
    // The LV_EVENT_DELETE callbacks on child objects will be triggered correctly.
    lv_obj_clean(container);

    lv_obj_t* title_label = lv_label_create(container);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(title_label, "Speaker Test");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    info_label = lv_label_create(container);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label);
    lv_label_set_text(info_label, "Press OK to select\na .wav audio file.");

    // Register handlers for this specific view state.
    setup_initial_button_handlers();
}

void SpeakerTestView::show_file_explorer() {
    lv_obj_clean(container);

    // This container will host the explorer and holds the cleanup callback.
    file_explorer_host_container = lv_obj_create(container);
    lv_obj_remove_style_all(file_explorer_host_container);
    lv_obj_set_size(file_explorer_host_container, LV_PCT(100), LV_PCT(100));

    // Attach the cleanup function to the container's delete event.
    lv_obj_add_event_cb(file_explorer_host_container, explorer_cleanup_event_cb, LV_EVENT_DELETE, nullptr);

    // Create the file_explorer inside the container that now has the callback.
    // Pass our static C-style callbacks.
    file_explorer_create(
        file_explorer_host_container,
        sd_manager_get_mount_point(),
        audio_file_selected_cb_c,
        nullptr, // No long press action
        nullptr, // No create action
        explorer_exit_cb_c
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
        
        // Clean the current screen. This will trigger the explorer_cleanup_event_cb,
        // which correctly destroys the file_explorer instance.
        lv_obj_clean(container);

        // The audio player component takes over the screen.
        audio_player_component_create(container, path, player_exit_cb_c);
    } else {
        ESP_LOGW(TAG, "File selected is not a .wav file: %s", path);
        // Optional: show a message to the user.
        // For now, we just ignore the selection and stay in the explorer.
    }
}

void SpeakerTestView::on_explorer_exit_from_root() {
    ESP_LOGI(TAG, "Exited file explorer from root. Returning to initial view.");
    create_initial_view();
}

void SpeakerTestView::on_player_exit() {
    ESP_LOGI(TAG, "Exiting audio player, returning to initial speaker test view.");
    // The player component is an LVGL child of the container, so it will be
    // automatically deleted by the lv_obj_clean call inside create_initial_view.
    create_initial_view();
}

// --- Static Callbacks for Button Manager (Bridge to instance methods) ---
void SpeakerTestView::initial_ok_press_cb(void* user_data) {
    static_cast<SpeakerTestView*>(user_data)->on_initial_ok_press();
}

void SpeakerTestView::initial_cancel_press_cb(void* user_data) {
    static_cast<SpeakerTestView*>(user_data)->on_initial_cancel_press();
}

// --- Static Callbacks for C Components (Workaround using s_instance) ---
void SpeakerTestView::audio_file_selected_cb_c(const char* path) {
    if (s_instance) s_instance->on_audio_file_selected(path);
}

void SpeakerTestView::explorer_exit_cb_c() {
    if (s_instance) s_instance->on_explorer_exit_from_root();
}

void SpeakerTestView::player_exit_cb_c() {
    if (s_instance) s_instance->on_player_exit();
}

/**
 * CRITICAL: This ensures that the C-style file explorer component cleans
 * up its non-LVGL resources (like timers and allocated memory) when the
 * view is switched.
 */
void SpeakerTestView::explorer_cleanup_event_cb(lv_event_t * e) {
    ESP_LOGD(TAG, "Explorer host container deleted. Calling file_explorer_destroy().");
    file_explorer_destroy();
    if (s_instance) {
        s_instance->file_explorer_host_container = nullptr; // Clear the dangling pointer
    }
}