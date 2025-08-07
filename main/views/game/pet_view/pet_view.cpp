#include "pet_view.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "views/view_manager.h"
#include "esp_log.h"
#include <time.h>
#include "lvgl.h"

static const char* TAG = "PET_VIEW";

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
    lv_obj_set_style_margin_bottom(title, 20, 0);

    // Pet "Image" placeholder
    pet_display_obj = lv_obj_create(parent);
    lv_obj_set_size(pet_display_obj, 100, 100);
    lv_obj_set_style_radius(pet_display_obj, 8, 0);

    // Pet Info Labels
    pet_name_label = lv_label_create(parent);
    lv_obj_set_style_text_align(pet_name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(pet_name_label, 15, 0);

    pet_points_label = lv_label_create(parent);
    lv_obj_set_style_text_align(pet_points_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(pet_points_label, 5, 0);
    
    pet_cycle_label = lv_label_create(parent);
    lv_obj_set_style_text_align(pet_cycle_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(pet_cycle_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_margin_top(pet_cycle_label, 10, 0);
}

void PetView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, add_points_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, back_button_cb, true, this);
}

void PetView::update_view() {
    auto& pet_manager = PetManager::get_instance();
    pet_manager.update_state();
    PetState state = pet_manager.get_current_pet_state();
    
    // Pass the entire state object to the display name function.
    lv_label_set_text(pet_name_label, pet_manager.get_pet_display_name(state).c_str());
    lv_label_set_text_fmt(pet_points_label, "Care Points: %lu", state.care_points);

    // Update the color of the placeholder based on pet type
    lv_color_t color;
    switch(state.type) {
        case PetType::FLUFFLE: color = lv_palette_main(LV_PALETTE_LIGHT_BLUE); break;
        case PetType::ROCKY:   color = lv_palette_main(LV_PALETTE_GREY); break;
        case PetType::SPROUT:  color = lv_palette_main(LV_PALETTE_GREEN); break;
        case PetType::EMBER:   color = lv_palette_main(LV_PALETTE_RED); break;
        case PetType::AQUABUB: color = lv_palette_main(LV_PALETTE_BLUE); break;
        default:               color = lv_palette_main(LV_PALETTE_NONE); break;
    }
    lv_obj_set_style_bg_color(pet_display_obj, color, 0);
    
    // Display cycle start date
    char date_buf[20];
    struct tm timeinfo;
    // Use the corrected member name: cycle_start_timestamp
    localtime_r(&state.cycle_start_timestamp, &timeinfo);
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &timeinfo);
    lv_label_set_text_fmt(pet_cycle_label, "Cycle Started:\n%s", date_buf);
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