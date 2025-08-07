#include "pet_view.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "views/view_manager.h"
#include "esp_log.h"
#include <time.h>
#include <string>
#include <cstdio>
#include "lvgl.h"

static const char* TAG = "PET_VIEW";

/**
 * @brief Formats a duration in seconds into a human-readable string.
 * Example: 90061 seconds -> "1d 1h 1m"
 * @param buf The output buffer for the string.
 * @param len The size of the output buffer.
 * @param seconds The total number of seconds to format.
 */
static void format_time_remaining(char* buf, size_t len, time_t seconds) {
    if (seconds <= 0) {
        snprintf(buf, len, "Final Stage");
        return;
    }

    int days = seconds / 86400;
    seconds %= 86400;
    int hours = seconds / 3600;
    seconds %= 3600;
    int minutes = seconds / 60;

    if (days > 0) {
        snprintf(buf, len, "Next stage in: %dd %dh %dm", days, hours, minutes);
    } else if (hours > 0) {
        snprintf(buf, len, "Next stage in: %dh %dm", hours, minutes);
    } else {
        snprintf(buf, len, "Next stage in: %dm", minutes);
    }
}


PetView::PetView() : update_timer(nullptr) {
    ESP_LOGI(TAG, "PetView constructed");
}

PetView::~PetView() {
    if (update_timer) {
        lv_timer_delete(update_timer);
    }
    ESP_LOGI(TAG, "PetView destructed");
}

void PetView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(container);
    
    setup_ui(container);
    setup_button_handlers();

    update_view(); // Initial update
    update_timer = lv_timer_create(update_view_cb, 5000, this); // Update every 5 seconds
}

void PetView::setup_ui(lv_obj_t* parent) {
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Pet Status");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_bottom(title, 10, 0);

    // Pet Image
    pet_display_obj = lv_image_create(parent);
    lv_obj_set_size(pet_display_obj, 64, 64); // Use a 64x64 area for the sprite

    // Pet Info Labels
    pet_name_label = lv_label_create(parent);
    lv_obj_set_style_text_align(pet_name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(pet_name_label, 10, 0);

    pet_points_label = lv_label_create(parent);
    lv_obj_set_style_text_align(pet_points_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(pet_points_label, 5, 0);
    
    pet_time_label = lv_label_create(parent);
    lv_obj_set_style_text_align(pet_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(pet_time_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_margin_top(pet_time_label, 10, 0);

    pet_cycle_label = lv_label_create(parent);
    lv_obj_set_style_text_align(pet_cycle_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(pet_cycle_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_margin_top(pet_cycle_label, 5, 0);
}

void PetView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, add_points_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, back_button_cb, true, this);
}

void PetView::update_view() {
    // First, check if SD card is available, otherwise show an error message.
    if (!sd_manager_check_ready()) {
        lv_img_set_src(pet_display_obj, LV_SYMBOL_SD_CARD " " LV_SYMBOL_WARNING);
        lv_label_set_text(pet_name_label, "SD Card Error");
        lv_label_set_text(pet_points_label, "Cannot load pet data");
        lv_label_set_text(pet_time_label, "");
        lv_label_set_text(pet_cycle_label, "");
        return;
    }

    auto& pet_manager = PetManager::get_instance();
    pet_manager.update_state();
    PetState state = pet_manager.get_current_pet_state();
    
    // --- Update Sprite ---
    std::string sprite_path = pet_manager.get_current_pet_sprite_path();
    if (!sprite_path.empty()) {
        lv_img_set_src(pet_display_obj, sprite_path.c_str());
        // Egg sprite is 16x16, pet sprite is 32x32. Zoom to make them 64x64 on screen.
        if (state.stage == PetStage::EGG) {
            lv_img_set_zoom(pet_display_obj, LV_ZOOM_NONE * 4); // 16px * 4 = 64px
        } else {
            lv_img_set_zoom(pet_display_obj, LV_ZOOM_NONE * 2); // 32px * 2 = 64px
        }
    } else {
        lv_img_set_src(pet_display_obj, LV_SYMBOL_WARNING); // Fallback icon
        lv_img_set_zoom(pet_display_obj, LV_ZOOM_NONE);
    }
    
    // --- Update Labels ---
    lv_label_set_text(pet_name_label, pet_manager.get_pet_display_name(state).c_str());
    lv_label_set_text_fmt(pet_points_label, "Care Points: %lu", state.care_points);

    // --- Update Time to Next Stage ---
    time_t time_left = pet_manager.get_time_to_next_stage(state);
    char time_buf[64];
    format_time_remaining(time_buf, sizeof(time_buf), time_left);
    lv_label_set_text(pet_time_label, time_buf);

    // Display cycle start date
    char date_buf[20];
    struct tm timeinfo;
    localtime_r(&state.cycle_start_timestamp, &timeinfo);
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &timeinfo);
    lv_label_set_text_fmt(pet_cycle_label, "Cycle Started: %s", date_buf);
}

void PetView::add_care_points() {
    ESP_LOGI(TAG, "OK button pressed. Adding 10 care points.");
    PetManager::get_instance().add_care_points(10);
    update_view(); // Update UI immediately after adding points
}

void PetView::go_back_to_menu() {
    // Pause the timer when leaving the view to save resources
    if(update_timer) lv_timer_pause(update_timer);
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks ---

void PetView::update_view_cb(lv_timer_t* timer) {
    auto* instance = static_cast<PetView*>(lv_timer_get_user_data(timer));
    if (instance) {
        instance->update_view();
    }
}

void PetView::add_points_cb(void* user_data) {
    static_cast<PetView*>(user_data)->add_care_points();
}

void PetView::back_button_cb(void* user_data) {
    static_cast<PetView*>(user_data)->go_back_to_menu();
}