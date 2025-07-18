#include "voice_note_player_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/stt_manager/stt_manager.h"
#include "components/file_explorer/file_explorer.h"
#include "components/audio_player_component/audio_player_component.h"
#include "components/text_viewer/text_viewer.h"
#include "esp_log.h"
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "VOICE_NOTE_PLAYER_VIEW";
static const char *NOTES_DIR = "/sdcard/notes";

// --- State ---
static lv_obj_t *view_parent = NULL;
static lv_obj_t *loading_indicator = NULL;

// --- Action Menu State ---
static lv_obj_t *action_menu_container = NULL;
static lv_group_t *action_menu_group = NULL;
static char selected_item_path[256];
static lv_style_t style_action_menu_focused;

// --- Struct for passing data to the LVGL async call ---
typedef struct {
    bool success;
    char* result_text;
} transcription_result_t;

// --- Prototypes ---
static void show_file_explorer();
static void destroy_action_menu_internal(bool refresh_explorer);
static void on_transcription_complete(bool success, char* result);
static void on_transcription_complete_ui_thread(void *user_data);
static void on_viewer_exit();
static void explorer_cleanup_cb(lv_event_t * e);

// --- Loading Indicator ---
static void show_loading_indicator(const char* text) {
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

static void hide_loading_indicator() {
    if (loading_indicator) {
        lv_obj_del(loading_indicator);
        loading_indicator = NULL;
    }
}

// --- Action Menu ---
static void handle_action_menu_left_press(void* user_data) {
    if (action_menu_group) lv_group_focus_prev(action_menu_group);
}
static void handle_action_menu_right_press(void* user_data) {
    if (action_menu_group) lv_group_focus_next(action_menu_group);
}
static void handle_action_menu_cancel(void* user_data) {
    destroy_action_menu_internal(false);
}

static void handle_action_menu_ok(void* user_data) {
    if (!action_menu_group) return;

    lv_obj_t* selected_btn = lv_group_get_focused(action_menu_group);
    if (!selected_btn) return;

    lv_obj_t* list = lv_obj_get_parent(selected_btn);
    const char* action_text = lv_list_get_button_text(list, selected_btn);
    ESP_LOGI(TAG, "Action '%s' selected for: %s", action_text, selected_item_path);

    if (strcmp(action_text, "Delete") == 0) {
        sd_manager_delete_item(selected_item_path);
        destroy_action_menu_internal(true);
    } else if (strcmp(action_text, "Transcribe") == 0) {
        if (!wifi_manager_is_connected()) {
             wifi_manager_init_sta(); // Attempt to connect if not already
        }
        destroy_action_menu_internal(false);
        show_loading_indicator("Transcribing...");
        if (!stt_manager_transcribe(selected_item_path, on_transcription_complete)) {
            hide_loading_indicator();
            ESP_LOGE(TAG, "Failed to start transcription task.");
            // On failure, return to the explorer so the user can try again.
            show_file_explorer();
        }
    } else {
        destroy_action_menu_internal(false);
    }
}

static void create_action_menu(const char* path) {
    if (action_menu_container) return;
    ESP_LOGI(TAG, "Creating action menu for: %s", path);
    strncpy(selected_item_path, path, sizeof(selected_item_path) - 1);
    
    file_explorer_set_input_active(false);

    lv_style_init(&style_action_menu_focused);
    lv_style_set_bg_color(&style_action_menu_focused, lv_palette_main(LV_PALETTE_BLUE));

    action_menu_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(action_menu_container);
    lv_obj_set_size(action_menu_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(action_menu_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(action_menu_container, LV_OPA_50, 0);

    lv_obj_t* menu_box = lv_obj_create(action_menu_container);
    lv_obj_set_width(menu_box, lv_pct(80));
    lv_obj_set_height(menu_box, LV_SIZE_CONTENT);
    lv_obj_center(menu_box);

    lv_obj_t* list = lv_list_create(menu_box);
    lv_obj_set_size(list, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_center(list);

    action_menu_group = lv_group_create();

    const char* actions[][2] = {
        {LV_SYMBOL_TRASH, "Delete"},
        {LV_SYMBOL_EDIT, "Transcribe"}
    };
    for(auto const& action : actions) {
        lv_obj_t* btn = lv_list_add_button(list, action[0], action[1]);
        lv_obj_add_style(btn, &style_action_menu_focused, LV_STATE_FOCUSED);
        lv_group_add_obj(action_menu_group, btn);
    }

    lv_group_set_default(action_menu_group);
    if(lv_obj_get_child_count(list) > 0){
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_action_menu_ok, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_action_menu_cancel, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, handle_action_menu_left_press, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, handle_action_menu_right_press, true, nullptr);
}

static void destroy_action_menu_internal(bool refresh_explorer) {
    if (action_menu_container) {
        if(action_menu_group) {
            if (lv_group_get_default() == action_menu_group) lv_group_set_default(NULL);
            lv_group_del(action_menu_group);
            action_menu_group = NULL;
        }
        lv_obj_del(action_menu_container);
        action_menu_container = NULL;
        file_explorer_set_input_active(true);
        if (refresh_explorer) file_explorer_refresh();
    }
}

// --- Component and Asynchronous Callbacks ---

// Callback from text_viewer when user exits
static void on_viewer_exit() {
    // Return to the file explorer. show_file_explorer() will clean the screen.
    show_file_explorer();
}

// This function runs on the LVGL UI thread after transcription is complete.
static void on_transcription_complete_ui_thread(void *user_data) {
    transcription_result_t* result_data = (transcription_result_t*)user_data;
    
    hide_loading_indicator();
    
    if(result_data->success) {
        ESP_LOGI(TAG, "UI THREAD: Transcription success. Showing result.");
        lv_obj_clean(view_parent);
        // text_viewer_create takes ownership of the result_data->result_text pointer
        text_viewer_create(view_parent, "Transcription", result_data->result_text, on_viewer_exit);
    } else {
        ESP_LOGE(TAG, "UI THREAD: Transcription failed: %s", result_data->result_text);
        // The text viewer is not created, so we must free the error message buffer.
        free(result_data->result_text);
        // Here you could show an error popup. For now, just return to the explorer.
        show_file_explorer();
    }

    // Free the data container struct
    free(result_data);
}

// This callback runs in the STT manager's task thread. Its only job
// is to package the data and post it to the LVGL thread.
static void on_transcription_complete(bool success, char* result) {
    auto* result_data = (transcription_result_t*)malloc(sizeof(transcription_result_t));
    if (result_data) {
        result_data->success = success;
        result_data->result_text = result; // The result pointer ownership is transferred
        lv_async_call(on_transcription_complete_ui_thread, result_data);
    } else {
        ESP_LOGE(TAG, "Failed to allocate memory for async call, result will be lost.");
        free(result); // Prevent memory leak if async call fails
    }
}

// Callback from audio_player_component when user exits
static void on_player_exit() {
    show_file_explorer();
}

// Callback when a file is selected in the explorer
static void on_audio_file_selected(const char *path) {
    // Clean the screen. This will trigger the explorer's cleanup callback automatically.
    lv_obj_clean(view_parent);
    // Create the audio player component.
    audio_player_component_create(view_parent, path, on_player_exit);
}

// Callback for long-press on a file in the explorer
static void on_file_long_pressed(const char *path) {
    create_action_menu(path);
}

// Callback when user exits the explorer by backing out of the root directory
static void on_explorer_exit() {
    // When exiting, the view manager will clean the screen, which triggers
    // the explorer's cleanup callback. We just need to load the previous view.
    view_manager_load_view(VIEW_ID_VOICE_NOTE);
}

// Robust Cleanup for File Explorer
// This event callback is attached to the explorer's container and ensures
// that file_explorer_destroy() is always called when the container is deleted,
// regardless of how the view is closed.
static void explorer_cleanup_cb(lv_event_t * e) {
    ESP_LOGI(TAG, "Explorer container deleted. Calling file_explorer_destroy() to free resources.");
    file_explorer_destroy();
}

// --- Main UI Creation Logic ---

static void show_file_explorer() {
    lv_obj_clean(view_parent);
    
    struct stat st;
    if (stat(NOTES_DIR, &st) != 0) {
        lv_obj_t* label = lv_label_create(view_parent);
        lv_label_set_text(label, "No voice notes found.\n\nPress Cancel to go back.");
        lv_obj_center(label);
        button_manager_unregister_view_handlers();
        button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, [](void* d){ on_explorer_exit(); }, true, nullptr);
        return;
    }

    // This container will host the explorer and holds the cleanup callback.
    lv_obj_t * explorer_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(100), lv_pct(100));
    
    // Attach the cleanup function to the container's delete event
    lv_obj_add_event_cb(explorer_container, explorer_cleanup_cb, LV_EVENT_DELETE, nullptr);

    file_explorer_create(
        explorer_container, 
        NOTES_DIR, 
        on_audio_file_selected, 
        on_file_long_pressed, 
        NULL, 
        on_explorer_exit
    );
}

// --- Public Entry Point ---
void voice_note_player_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Voice Note Player View");
    view_parent = parent;
    show_file_explorer();
}