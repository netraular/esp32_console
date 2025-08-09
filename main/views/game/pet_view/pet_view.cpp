#include "pet_view.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "components/popup_manager/popup_manager.h"
#include "views/view_manager.h"
#include "esp_log.h"
#include <time.h>
#include <string>
#include <cstdio>
#include "lvgl.h"

static const char* TAG = "PET_VIEW";

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
    seconds %= 60;

    if (days > 0) snprintf(buf, len, "Next stage in: %dd %dh %dm", days, hours, minutes);
    else if (hours > 0) snprintf(buf, len, "Next stage in: %dh %dm", hours, minutes);
    else if (minutes > 0) snprintf(buf, len, "Next stage in: %dm %lds", minutes, (long)seconds);
    else snprintf(buf, len, "Next stage in: %lds", (long)seconds);
}

static void format_hatch_time(char* buf, size_t len, time_t seconds) {
    if (seconds <= 0) {
        snprintf(buf, len, "Hatching!");
        return;
    }
    int minutes = seconds / 60;
    seconds %= 60;
    snprintf(buf, len, "Hatches in: %dm %lds", minutes, (long)seconds);
}

PetView::PetView() : update_timer(nullptr) {
    ESP_LOGI(TAG, "PetView constructed");
}

PetView::~PetView() {
    if (update_timer) lv_timer_delete(update_timer);
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

    update_view();
    // Update more frequently to show seconds countdown
    update_timer = lv_timer_create(update_view_cb, 1000, this);
}

void PetView::setup_ui(lv_obj_t* parent) {
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Pet Status");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_bottom(title, 10, 0);

    pet_display_obj = lv_image_create(parent);
    lv_obj_set_size(pet_display_obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_image_set_antialias(pet_display_obj, false);
    lv_obj_align(pet_display_obj, LV_ALIGN_CENTER, 0, 0);


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
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_LONG_PRESS_START, force_new_pet_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, back_button_cb, true, this);
}

void PetView::update_view() {
    if (!sd_manager_check_ready()) {
        lv_image_set_src(pet_display_obj, LV_SYMBOL_SD_CARD " " LV_SYMBOL_WARNING);
        lv_label_set_text(pet_name_label, "SD Card Error");
        lv_label_set_text(pet_points_label, "Cannot load pet data");
        lv_label_set_text(pet_time_label, "");
        lv_label_set_text(pet_cycle_label, "");
        return;
    }

    auto& pet_manager = PetManager::get_instance();
    pet_manager.update_state();
    PetState state = pet_manager.get_current_pet_state();
    
    std::string sprite_path = pet_manager.get_current_pet_sprite_path();
    if (!sprite_path.empty()) {
        lv_image_set_src(pet_display_obj, sprite_path.c_str());
    } else {
        lv_image_set_src(pet_display_obj, LV_SYMBOL_WARNING);
    }
    
    lv_label_set_text(pet_name_label, pet_manager.get_pet_display_name(state).c_str());

    // Update labels based on whether it's an egg or a pet
    char time_buf[64];
    if (pet_manager.is_in_egg_stage()) {
        lv_label_set_text(pet_points_label, ""); // No points for an egg
        time_t time_left = pet_manager.get_time_to_hatch();
        format_hatch_time(time_buf, sizeof(time_buf), time_left);
        lv_label_set_text(pet_time_label, time_buf);
        lv_label_set_text(pet_cycle_label, "");
    } else {
        lv_label_set_text_fmt(pet_points_label, "Care Points: %lu", state.care_points);
        time_t time_left = pet_manager.get_time_to_next_stage(state);
        format_time_remaining(time_buf, sizeof(time_buf), time_left);
        lv_label_set_text(pet_time_label, time_buf);
        
        char date_buf[20];
        struct tm timeinfo;
        localtime_r(&state.cycle_start_timestamp, &timeinfo);
        strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &timeinfo);
        lv_label_set_text_fmt(pet_cycle_label, "Hatched: %s", date_buf);
    }
}

void PetView::add_care_points() {
    ESP_LOGI(TAG, "OK button pressed. Adding 10 care points.");
    PetManager::get_instance().add_care_points(10);
    update_view();
}

void PetView::on_force_new_pet() {
    popup_manager_show_confirmation(
        "New Egg?", "This will abandon your current pet.\nAre you sure?", "Confirm", "Cancel",
        force_new_pet_popup_cb, this
    );
}

void PetView::handle_force_new_pet_result(popup_result_t result) {
    if (result == POPUP_RESULT_PRIMARY) {
        PetManager::get_instance().force_new_cycle();
        update_view();
    }
    setup_button_handlers(); 
}

void PetView::go_back_to_menu() {
    if(update_timer) lv_timer_pause(update_timer);
    view_manager_load_view(VIEW_ID_MENU);
}

void PetView::update_view_cb(lv_timer_t* timer) {
    static_cast<PetView*>(lv_timer_get_user_data(timer))->update_view();
}

void PetView::add_points_cb(void* user_data) {
    static_cast<PetView*>(user_data)->add_care_points();
}

void PetView::force_new_pet_cb(void* user_data) {
    static_cast<PetView*>(user_data)->on_force_new_pet();
}

void PetView::force_new_pet_popup_cb(popup_result_t result, void* user_data) {
    static_cast<PetView*>(user_data)->handle_force_new_pet_result(result);
}

void PetView::back_button_cb(void* user_data) {
    static_cast<PetView*>(user_data)->go_back_to_menu();
}