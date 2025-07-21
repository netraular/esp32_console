#include "volume_tester_view.h"
#include "../view_manager.h"
#include "config.h"

static const char* TAG = "VOLUME_TESTER_VIEW";
const char* VolumeTesterView::TEST_SOUND_PATH = "/sdcard/sounds/test.wav";

// --- Lifecycle Methods ---

VolumeTesterView::VolumeTesterView() {
    ESP_LOGI(TAG, "VolumeTesterView constructed");
    // Initialize state
    current_state = ViewState::CHECKING_SD;
    is_playing = false;
    audio_check_timer = nullptr;
    volume_label = nullptr;
    status_label = nullptr;
}

VolumeTesterView::~VolumeTesterView() {
    ESP_LOGI(TAG, "VolumeTesterView destructed, cleaning up resources.");
    
    if (audio_check_timer) {
        lv_timer_del(audio_check_timer);
        audio_check_timer = nullptr;
    }
    audio_manager_stop();
    audio_manager_set_volume_physical(SAFE_DEFAULT_VOLUME);
}

void VolumeTesterView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Volume Tester View");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_center(container);

    // Initial setup: check for SD card and display the appropriate UI
    setup_ui();
}

// --- UI & Handler Setup ---

void VolumeTesterView::setup_ui() {
    // Clean container to ensure no previous UI elements exist
    lv_obj_clean(container);
    // Unregister all previous button handlers to start fresh
    button_manager_unregister_view_handlers();

    if (sd_manager_check_ready()) {
        current_state = ViewState::READY;
        show_ready_ui();
    } else {
        current_state = ViewState::SD_ERROR;
        show_error_ui();
    }
}

void VolumeTesterView::show_ready_ui() {
    // Use a flex layout for easy alignment.
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container, 10, 0);
    lv_obj_set_style_pad_gap(container, 15, 0);

    lv_obj_t* title_label = lv_label_create(container);
    lv_label_set_text(title_label, "Volume Tester");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);

    volume_label = lv_label_create(container);
    lv_obj_set_style_text_font(volume_label, &lv_font_montserrat_48, 0);
    update_volume_label();

    status_label = lv_label_create(container);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(status_label, "Press OK to Play");

    lv_obj_t* info_label = lv_label_create(container);
    lv_label_set_text(info_label,
                      "Find max safe volume.\n\n"
                      LV_SYMBOL_LEFT " / " LV_SYMBOL_RIGHT " : Adjust Volume\n"
                      "OK : Play / Stop\n"
                      "CANCEL : Exit");
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(info_label, 4, 0);

    // Register handlers for the "Ready" state
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, VolumeTesterView::volume_down_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, VolumeTesterView::volume_up_cb,   true, this);
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, VolumeTesterView::ok_press_cb,    true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, VolumeTesterView::exit_press_cb,  true, this);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_LONG_PRESS_HOLD, VolumeTesterView::volume_down_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_LONG_PRESS_HOLD, VolumeTesterView::volume_up_cb,   true, this);
}

void VolumeTesterView::show_error_ui() {
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container, 10, 0);
    lv_obj_set_style_pad_gap(container, 20, 0);
    
    lv_obj_t* icon_label = lv_label_create(container);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);
    lv_label_set_text(icon_label, LV_SYMBOL_SD_CARD " " LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);

    lv_obj_t* text_label = lv_label_create(container);
    lv_label_set_text(text_label, "SD Card Not Found\n\nInsert card and press OK to retry.");
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);

    // Register handlers for the "Error" state
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, VolumeTesterView::ok_press_cb,   true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, VolumeTesterView::exit_press_cb, true, this);
}

// --- UI Logic & Instance Methods ---

void VolumeTesterView::update_volume_label() {
    if (volume_label) {
        uint8_t vol = audio_manager_get_volume();
        lv_label_set_text_fmt(volume_label, "%u%%", vol);
    }
}

void VolumeTesterView::on_play_toggle() {
    if (is_playing) {
        // --- Stop Playback ---
        ESP_LOGI(TAG, "OK pressed: Stopping playback.");
        audio_manager_stop();
        if (audio_check_timer) {
            lv_timer_del(audio_check_timer);
            audio_check_timer = nullptr;
        }
        lv_label_set_text(status_label, "Press OK to Play");
        lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
        is_playing = false;
    } else {
        // --- Start Playback ---
        ESP_LOGI(TAG, "OK pressed: Starting playback.");
        if (audio_manager_play(TEST_SOUND_PATH)) {
            audio_check_timer = lv_timer_create(audio_check_timer_cb, 500, this);
            lv_label_set_text(status_label, "Playing...");
            lv_obj_set_style_text_color(status_label, lv_palette_main(LV_PALETTE_GREEN), 0);
            is_playing = true;
        } else {
            lv_label_set_text(status_label, "Error: Can't play file!");
            lv_obj_set_style_text_color(status_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }
}

void VolumeTesterView::on_retry_check() {
    ESP_LOGI(TAG, "Retrying SD card check...");
    // This will clean the screen and re-run the check logic.
    setup_ui();
}

void VolumeTesterView::on_exit_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

void VolumeTesterView::on_volume_up() {
    uint8_t current_vol = audio_manager_get_volume();
    if (current_vol < 100) {
        audio_manager_set_volume_physical(current_vol + 1);
    }
    update_volume_label();
}

void VolumeTesterView::on_volume_down() {
    uint8_t current_vol = audio_manager_get_volume();
    if (current_vol > 0) {
        audio_manager_set_volume_physical(current_vol - 1);
    }
    update_volume_label();
}

// --- Static Callbacks (Bridges) ---

void VolumeTesterView::audio_check_timer_cb(lv_timer_t* timer) {
    auto* instance = static_cast<VolumeTesterView*>(lv_timer_get_user_data(timer));
    if (!instance) return;

    audio_player_state_t state = audio_manager_get_state();
    if (state == AUDIO_STATE_STOPPED || state == AUDIO_STATE_ERROR) {
        ESP_LOGI(TAG, "Audio stopped, re-playing for loop effect.");
        if (!audio_manager_play(TEST_SOUND_PATH)) {
            if (instance->status_label) {
                lv_label_set_text(instance->status_label, "Error re-playing!");
            }
        }
    }
}

void VolumeTesterView::ok_press_cb(void* user_data) {
    auto* instance = static_cast<VolumeTesterView*>(user_data);
    // The action of the OK button depends on the current view state
    if (instance->current_state == ViewState::READY) {
        instance->on_play_toggle();
    } else if (instance->current_state == ViewState::SD_ERROR) {
        instance->on_retry_check();
    }
}

void VolumeTesterView::exit_press_cb(void* user_data) {
    static_cast<VolumeTesterView*>(user_data)->on_exit_press();
}

void VolumeTesterView::volume_up_cb(void* user_data) {
    static_cast<VolumeTesterView*>(user_data)->on_volume_up();
}

void VolumeTesterView::volume_down_cb(void* user_data) {
    static_cast<VolumeTesterView*>(user_data)->on_volume_down();
}