#include "voice_note_player_view.h"
#include "views/view_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/stt_manager/stt_manager.h"
#include "components/audio_player_component/audio_player_component.h"
#include "components/text_viewer/text_viewer.h"
#include "esp_log.h"
#include <string>
#include <sys/stat.h>
#include <memory> 
#include <string.h> // Include for strdup

static const char *TAG = "VOICE_NOTE_PLAYER_VIEW";
static const char *NOTES_DIR = "/sdcard/notes";

// Struct for passing data from background task to LVGL UI thread.
// Using std::string makes it memory-safe.
struct transcription_result_data_t {
    bool success;
    std::string result_text;
    VoiceNotePlayerView* instance; // Pass context back to the UI thread
};


// --- Lifecycle Methods ---
VoiceNotePlayerView::VoiceNotePlayerView() : loading_indicator(nullptr), action_menu_container(nullptr), file_explorer_host_container(nullptr), action_menu_group(nullptr), styles_initialized(false) {
    ESP_LOGI(TAG, "VoiceNotePlayerView constructed");
    selected_item_path[0] = '\0';
}

VoiceNotePlayerView::~VoiceNotePlayerView() {
    ESP_LOGI(TAG, "VoiceNotePlayerView destructed");
    destroy_action_menu(false); 
    reset_action_menu_styles(); 
    hide_loading_indicator();
}

void VoiceNotePlayerView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Voice Note Player View");
    container = parent;
    show_file_explorer();
}

// --- UI & State Management ---

void VoiceNotePlayerView::show_loading_indicator(const char* text) {
    if (loading_indicator) return;

    loading_indicator = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(loading_indicator);
    lv_obj_set_size(loading_indicator, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(loading_indicator, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(loading_indicator, LV_OPA_70, 0);
    lv_obj_clear_flag(loading_indicator, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *spinner = lv_spinner_create(loading_indicator);
    lv_obj_center(spinner);
    lv_obj_t* label = lv_label_create(loading_indicator);
    lv_label_set_text(label, text);
    lv_obj_align_to(label, spinner, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

void VoiceNotePlayerView::hide_loading_indicator() {
    if (loading_indicator) {
        lv_obj_del(loading_indicator);
        loading_indicator = nullptr;
    }
}

void VoiceNotePlayerView::show_file_explorer() {
    lv_obj_clean(container);

    struct stat st;
    if (stat(NOTES_DIR, &st) != 0) {
        lv_obj_t* label = lv_label_create(container);
        lv_label_set_text(label, "No voice notes found.\n\nPress Cancel to go back.");
        lv_obj_center(label);
        button_manager_unregister_view_handlers();
        button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, [](void* user_data){ 
            if (user_data) static_cast<VoiceNotePlayerView*>(user_data)->on_explorer_exit(); 
        }, true, this);
        return;
    }

    file_explorer_host_container = lv_obj_create(container);
    lv_obj_remove_style_all(file_explorer_host_container);
    lv_obj_set_size(file_explorer_host_container, lv_pct(100), lv_pct(100));
    lv_obj_add_event_cb(file_explorer_host_container, explorer_cleanup_cb, LV_EVENT_DELETE, this);

    file_explorer_create(file_explorer_host_container, NOTES_DIR, 
                         audio_file_selected_cb_c, file_long_pressed_cb_c, 
                         nullptr, explorer_exit_cb_c, this);
}

// --- Action Menu ---

void VoiceNotePlayerView::create_action_menu(const char* path) {
    if (action_menu_container) return;
    ESP_LOGI(TAG, "Creating action menu for: %s", path);
    strncpy(selected_item_path, path, sizeof(selected_item_path) - 1);
    
    file_explorer_set_input_active(false);
    init_action_menu_styles();

    action_menu_container = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(action_menu_container);
    lv_obj_set_size(action_menu_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(action_menu_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(action_menu_container, LV_OPA_50, 0);

    lv_obj_t* list = lv_list_create(action_menu_container);
    lv_obj_set_size(list, 180, LV_SIZE_CONTENT);
    lv_obj_center(list);

    action_menu_group = lv_group_create();
    const char* actions[][2] = {{"Delete", LV_SYMBOL_TRASH}, {"Transcribe", LV_SYMBOL_EDIT}};
    for (auto const& action : actions) {
        lv_obj_t* btn = lv_list_add_button(list, action[1], action[0]);
        lv_obj_add_style(btn, &style_action_menu_focused, LV_STATE_FOCUSED);
        lv_group_add_obj(action_menu_group, btn);
    }

    if (lv_obj_get_child_count(list) > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }
    
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, action_menu_ok_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, action_menu_cancel_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, action_menu_left_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, action_menu_right_cb, true, this);
}

void VoiceNotePlayerView::destroy_action_menu(bool refresh_explorer) {
    if (!action_menu_container) return;

    if (action_menu_group) {
        lv_group_del(action_menu_group);
        action_menu_group = nullptr;
    }
    lv_obj_del(action_menu_container);
    action_menu_container = nullptr;

    file_explorer_set_input_active(true);
    if (refresh_explorer) file_explorer_refresh();
}

void VoiceNotePlayerView::init_action_menu_styles() {
    if (styles_initialized) return;
    lv_style_init(&style_action_menu_focused);
    lv_style_set_bg_color(&style_action_menu_focused, lv_palette_main(LV_PALETTE_BLUE));
    styles_initialized = true;
}

void VoiceNotePlayerView::reset_action_menu_styles() {
    if (styles_initialized) {
        lv_style_reset(&style_action_menu_focused);
        styles_initialized = false;
    }
}

// --- Instance Methods for Actions & Callbacks ---

void VoiceNotePlayerView::on_audio_file_selected(const char* path) {
    lv_obj_clean(container);
    audio_player_component_create(container, path, player_exit_cb_c, this);
}

void VoiceNotePlayerView::on_file_long_pressed(const char* path) {
    create_action_menu(path);
}

void VoiceNotePlayerView::on_explorer_exit() {
    view_manager_load_view(VIEW_ID_VOICE_NOTE);
}

void VoiceNotePlayerView::on_player_exit() {
    show_file_explorer();
}

void VoiceNotePlayerView::on_viewer_exit() {
    show_file_explorer();
}

void VoiceNotePlayerView::on_action_menu_ok() {
    if (!action_menu_group) return;
    lv_obj_t* selected_btn = lv_group_get_focused(action_menu_group);
    if (!selected_btn) return;

    const char* action_text = lv_list_get_button_text(lv_obj_get_parent(selected_btn), selected_btn);
    ESP_LOGI(TAG, "Action '%s' selected for: %s", action_text, selected_item_path);

    std::string path_str = selected_item_path;

    if (strcmp(action_text, "Delete") == 0) {
        sd_manager_delete_item(path_str.c_str());
        destroy_action_menu(true);
    } else if (strcmp(action_text, "Transcribe") == 0) {
        if (!wifi_manager_is_connected()) {
             wifi_manager_init_sta(); 
        }
        destroy_action_menu(false);
        show_loading_indicator("Transcribing...");
        
        // Create a lambda function for the callback.
        // It captures `this` to access the current view instance.
        auto stt_lambda_cb = [this](bool success, const std::string& result) {
            // This code runs in the context of the stt_manager task.
            // We need to pass the result to the UI thread safely.
            auto result_data = std::make_unique<transcription_result_data_t>();
            result_data->success = success;
            result_data->result_text = result;
            result_data->instance = this;

            // lv_async_call is thread-safe and will execute the function on the LVGL thread.
            lv_async_call(on_transcription_complete_ui_thread, result_data.release());
        };

        if (!stt_manager_transcribe(path_str, stt_lambda_cb)) {
            hide_loading_indicator();
            ESP_LOGE(TAG, "Failed to start transcription task.");
            show_file_explorer();
        }
    } else {
        destroy_action_menu(false);
    }
}

void VoiceNotePlayerView::on_action_menu_cancel() {
    destroy_action_menu(false);
}

void VoiceNotePlayerView::on_action_menu_nav(bool is_next) {
    if (!action_menu_group) return;
    if (is_next) lv_group_focus_next(action_menu_group);
    else lv_group_focus_prev(action_menu_group);
}


// --- Static Callbacks (Bridges) ---

void VoiceNotePlayerView::action_menu_ok_cb(void* user_data) { static_cast<VoiceNotePlayerView*>(user_data)->on_action_menu_ok(); }
void VoiceNotePlayerView::action_menu_cancel_cb(void* user_data) { static_cast<VoiceNotePlayerView*>(user_data)->on_action_menu_cancel(); }
void VoiceNotePlayerView::action_menu_left_cb(void* user_data) { static_cast<VoiceNotePlayerView*>(user_data)->on_action_menu_nav(false); }
void VoiceNotePlayerView::action_menu_right_cb(void* user_data) { static_cast<VoiceNotePlayerView*>(user_data)->on_action_menu_nav(true); }

void VoiceNotePlayerView::audio_file_selected_cb_c(const char* path, void* user_data) { if (user_data) static_cast<VoiceNotePlayerView*>(user_data)->on_audio_file_selected(path); }
void VoiceNotePlayerView::file_long_pressed_cb_c(const char* path, void* user_data) { if (user_data) static_cast<VoiceNotePlayerView*>(user_data)->on_file_long_pressed(path); }
void VoiceNotePlayerView::explorer_exit_cb_c(void* user_data) { if (user_data) static_cast<VoiceNotePlayerView*>(user_data)->on_explorer_exit(); }
void VoiceNotePlayerView::player_exit_cb_c(void* user_data) { if (user_data) static_cast<VoiceNotePlayerView*>(user_data)->on_player_exit(); }
void VoiceNotePlayerView::viewer_exit_cb_c(void* user_data) { if (user_data) static_cast<VoiceNotePlayerView*>(user_data)->on_viewer_exit(); }


void VoiceNotePlayerView::explorer_cleanup_cb(lv_event_t * e) {
    ESP_LOGD(TAG, "Explorer host container deleted. Calling file_explorer_destroy().");
    file_explorer_destroy();
    
    void* user_data = lv_event_get_user_data(e);
    if(user_data) {
        static_cast<VoiceNotePlayerView*>(user_data)->file_explorer_host_container = nullptr;
    }
}

void VoiceNotePlayerView::on_transcription_complete_ui_thread(void *user_data) {
    // Take ownership of the data with a unique_ptr to ensure it's deleted.
    std::unique_ptr<transcription_result_data_t> result_data(static_cast<transcription_result_data_t*>(user_data));
    
    VoiceNotePlayerView* instance = result_data->instance;

    if (!instance || !instance->container) {
        ESP_LOGE(TAG, "Player view instance or its container is null, cannot process transcription result.");
        return;
    }

    instance->hide_loading_indicator();
    
    // The text_viewer now owns the memory of the C-string.
    // We must convert the std::string to a dynamically allocated char*.
    char* content_for_viewer = strdup(result_data->result_text.c_str());
    if (!content_for_viewer) {
         ESP_LOGE(TAG, "Failed to allocate memory for text viewer content.");
         instance->show_file_explorer();
         return;
    }

    if(result_data->success) {
        ESP_LOGI(TAG, "UI THREAD: Transcription success. Showing result.");
        lv_obj_clean(instance->container);
        text_viewer_create(instance->container, "Transcription", content_for_viewer, viewer_exit_cb_c, instance);
    } else {
        ESP_LOGE(TAG, "UI THREAD: Transcription failed: %s", content_for_viewer);
        free(content_for_viewer); // Free the memory if not used by the viewer
        instance->show_file_explorer();
    }
}