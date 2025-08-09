#include "pet_hub_view.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "views/view_manager.h"
#include "esp_log.h"
#include <string>

static const char* TAG = "PET_HUB_VIEW";

PetHubView::PetHubView() {
    ESP_LOGI(TAG, "PetHubView constructed");
}

PetHubView::~PetHubView() {
    ESP_LOGI(TAG, "PetHubView destructed");
}

void PetHubView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(container);
    
    setup_ui(container);
    setup_button_handlers();

    // The main purpose of this view: log the collection status.
    log_collection_status();
}

void PetHubView::setup_ui(lv_obj_t* parent) {
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Pet Collection Log");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_bottom(title, 20, 0);

    lv_obj_t* info_label = lv_label_create(parent);
    lv_label_set_text(info_label, "Collection status has been printed\nto the debug console.\n\nPress Cancel to go back.");
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
}

void PetHubView::log_collection_status() {
    ESP_LOGI(TAG, "--- Pet Collection Status ---");
    auto& pet_manager = PetManager::get_instance();
    auto collection = pet_manager.get_collection();

    for (const auto& entry : collection) {
        std::string pet_name = pet_manager.get_pet_base_name(entry.type);
        const char* discovered_str = entry.discovered ? "YES" : "NO";
        const char* collected_str = entry.collected ? "YES" : "NO";

        ESP_LOGI(TAG, "Pet: %-10s | Discovered: %-3s | Collected: %-3s",
                 pet_name.c_str(), discovered_str, collected_str);
    }
    ESP_LOGI(TAG, "-----------------------------");
}


void PetHubView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, back_button_cb, true, this);
}

void PetHubView::go_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks ---
void PetHubView::back_button_cb(void* user_data) {
    static_cast<PetHubView*>(user_data)->go_back_to_menu();
}