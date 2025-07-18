#include "volume_tester_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "config.h"
#include "esp_log.h"
#include <new> // For std::nothrow

static const char* TAG = "VolumeTesterView";
static const char* const TEST_SOUND_PATH = "/sdcard/sounds/test.wav";

// --- State structure for the view ---
// Encapsulates all dynamic elements of the view for proper resource management.
typedef struct {
    lv_obj_t* volume_label;
    lv_obj_t* status_label;
    lv_timer_t* audio_check_timer;
    bool is_playing;
} volume_tester_data_t;

// --- Private Function Prototypes ---
static void update_volume_label(lv_obj_t* label);
static void audio_check_timer_cb(lv_timer_t* timer);
static void handle_ok_press(void* user_data);
static void handle_exit(void* user_data);
static void view_delete_cb(lv_event_t* e);
static void handle_volume_up(void* user_data);
static void handle_volume_down(void* user_data);


// --- UI and State Update Functions ---

// Updates the label that displays the volume percentage.
static void update_volume_label(lv_obj_t* label) {
    if (label) {
        // Use get_volume() which returns the actual physical value (0-100).
        uint8_t vol = audio_manager_get_volume();
        lv_label_set_text_fmt(label, "%u%%", vol);
    }
}

// Timer callback to loop the audio playback.
// It checks if the audio has stopped and restarts it.
static void audio_check_timer_cb(lv_timer_t* timer) {
    auto* data = static_cast<volume_tester_data_t*>(lv_timer_get_user_data(timer));
    if (!data) { // Defensive check
        return;
    }
    audio_player_state_t state = audio_manager_get_state();
    
    // If audio stopped (normally or due to an error), restart it.
    if (state == AUDIO_STATE_STOPPED || state == AUDIO_STATE_ERROR) {
        ESP_LOGI(TAG, "Audio stopped, re-playing for loop effect.");
        if (!audio_manager_play(TEST_SOUND_PATH)) {
            if (data->status_label) {
                lv_label_set_text(data->status_label, "Error re-playing!");
            }
        }
    }
}


// --- Button Handlers ---

// Increases volume and updates the UI.
static void handle_volume_up(void* user_data) {
    auto* data = static_cast<volume_tester_data_t*>(user_data);
    audio_manager_volume_up();
    update_volume_label(data->volume_label);
}

// Decreases volume and updates the UI.
static void handle_volume_down(void* user_data) {
    auto* data = static_cast<volume_tester_data_t*>(user_data);
    audio_manager_volume_down();
    update_volume_label(data->volume_label);
}

// Toggles audio playback on/off.
static void handle_ok_press(void* user_data) {
    auto* data = static_cast<volume_tester_data_t*>(user_data);

    if (data->is_playing) {
        // --- Stop Playback ---
        ESP_LOGI(TAG, "OK pressed: Stopping playback.");
        audio_manager_stop();
        if (data->audio_check_timer) {
            lv_timer_delete(data->audio_check_timer);
            data->audio_check_timer = nullptr;
        }
        lv_label_set_text(data->status_label, "Press OK to Play");
        lv_obj_set_style_text_color(data->status_label, lv_color_white(), 0);
        data->is_playing = false;
    } else {
        // --- Start Playback ---
        ESP_LOGI(TAG, "OK pressed: Starting playback.");
        if (audio_manager_play(TEST_SOUND_PATH)) {
            data->audio_check_timer = lv_timer_create(audio_check_timer_cb, 500, data);
            lv_label_set_text(data->status_label, "Playing...");
            lv_obj_set_style_text_color(data->status_label, lv_palette_main(LV_PALETTE_GREEN), 0);
            data->is_playing = true;
        } else {
            lv_label_set_text(data->status_label, "Error: Can't play file!");
            lv_obj_set_style_text_color(data->status_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }
}

// Exits the view and returns to the main menu.
static void handle_exit(void* user_data) {
    // Cleanup is handled by the LV_EVENT_DELETE callback, not here.
    view_manager_load_view(VIEW_ID_MENU);
}


// --- Resource Management ---

// Cleanup callback, triggered when the view's container is deleted.
// This is the key to preventing resource leaks.
static void view_delete_cb(lv_event_t* e) {
    ESP_LOGI(TAG, "Cleaning up Volume Tester View resources.");
    auto* data = static_cast<volume_tester_data_t*>(lv_event_get_user_data(e));
    if (data) {
        // 1. Delete the LVGL timer if it exists.
        if (data->audio_check_timer) {
            lv_timer_delete(data->audio_check_timer);
        }
        // 2. Stop any audio playback. This is crucial.
        audio_manager_stop();
        // 3. Restore a safe default volume when leaving this test view.
        //    15% is a reasonable, safe level that is audible but not damaging.
        audio_manager_set_volume_physical(15);
        // 4. Free the memory allocated for the state structure.
        delete data;
    }
}


// --- View Creation ---

void volume_tester_view_create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Volume Tester View");

    // Allocate the state structure for this view instance.
    auto* data = new(std::nothrow) volume_tester_data_t;
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for view data");
        return;
    }
    // Initialize state to known values.
    data->audio_check_timer = nullptr;
    data->is_playing = false;
    data->volume_label = nullptr;
    data->status_label = nullptr;
    
    // Create a main container for this view. All view elements will be its children.
    lv_obj_t* view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(view_container);
    lv_obj_set_size(view_container, lv_pct(100), lv_pct(100));
    lv_obj_center(view_container);
    // Register the cleanup callback on THIS container. It will be called when
    // view_manager executes lv_obj_clean() on the screen.
    lv_obj_add_event_cb(view_container, view_delete_cb, LV_EVENT_DELETE, data);

    // Use a flex layout for easy alignment.
    lv_obj_set_flex_flow(view_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(view_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(view_container, 10, 0);
    lv_obj_set_style_pad_gap(view_container, 15, 0);

    // Create all widgets as children of the view's container.
    lv_obj_t* title_label = lv_label_create(view_container);
    lv_label_set_text(title_label, "Volume Tester");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);

    data->volume_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(data->volume_label, &lv_font_montserrat_48, 0);
    update_volume_label(data->volume_label);

    data->status_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(data->status_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(data->status_label, "Press OK to Play");

    lv_obj_t* info_label = lv_label_create(view_container);
    lv_label_set_text(info_label,
                      "Find max safe volume.\n\n"
                      LV_SYMBOL_LEFT " / " LV_SYMBOL_RIGHT " : Adjust Volume\n"
                      "OK : Play / Stop\n"
                      "CANCEL : Exit");
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(info_label, 4, 0);

    // Register button handlers, passing the pointer to our state data.
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, handle_volume_down, true, data);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, handle_volume_up,   true, data);
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press,    true, data);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_exit,        true, data);
    
    // Also handle long press for faster volume changes.
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_LONG_PRESS_HOLD, handle_volume_down, true, data);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_LONG_PRESS_HOLD, handle_volume_up,   true, data);
}