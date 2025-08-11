#include "pet_collection_view.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "views/view_manager.h"
#include "esp_log.h"
#include <string>
#include <set>

static const char* TAG = "PET_COLL_VIEW";

PetCollectionView::PetCollectionView() {
    ESP_LOGI(TAG, "PetCollectionView constructed");
    // Initialize the style object. It will be configured in create().
    lv_style_init(&style_focus);
}

PetCollectionView::~PetCollectionView() {
    // Clean up the style object to prevent memory leaks
    lv_style_reset(&style_focus);
    ESP_LOGI(TAG, "PetCollectionView destructed");
}

void PetCollectionView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(container);

    // Configure the focus style for the selected tile
    lv_style_set_border_color(&style_focus, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_width(&style_focus, 2);
    lv_style_set_border_side(&style_focus, LV_BORDER_SIDE_FULL);
    lv_style_set_border_opa(&style_focus, LV_OPA_COVER);

    setup_ui(container);
    populate_container();
    setup_button_handlers();

    // Set initial selection
    if (!tile_items.empty()) {
        selected_index = 0;
        update_selection();
    }
}

void PetCollectionView::setup_ui(lv_obj_t* parent) {
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Pet Collection");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_top(title, 5, 0);
    lv_obj_set_style_margin_bottom(title, 5, 0);

    // Create a generic object to act as our scrollable list
    scrollable_container = lv_obj_create(parent);
    lv_obj_remove_style_all(scrollable_container);
    lv_obj_set_size(scrollable_container, LV_PCT(100), LV_PCT(85));
    lv_obj_set_flex_flow(scrollable_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scrollable_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(scrollable_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scrollable_container, LV_SCROLLBAR_MODE_AUTO);
}

void PetCollectionView::populate_container() {
    auto& pet_manager = PetManager::get_instance();
    auto collection_status = pet_manager.get_collection();
    bool sd_ready = sd_manager_check_ready();

    if (!sd_ready) {
        lv_obj_t* err_label = lv_label_create(scrollable_container);
        lv_label_set_text(err_label, "Error: SD Card not found.");
        return;
    }

    // Use a set to prevent adding the same pet more than once, ensuring robustness.
    std::set<PetId> added_pets;

    // Process each evolution line from the collection
    for (const auto& entry : collection_status) {
        PetId current_pet_id = entry.base_id;

        while (current_pet_id != PetId::NONE) {
            if (added_pets.count(current_pet_id)) {
                break; // Should not happen with current data, but good practice
            }
            
            const PetData* data = pet_manager.get_pet_data(current_pet_id);
            if (!data) break;

            create_pet_tile(data, entry);
            added_pets.insert(current_pet_id);

            current_pet_id = data->evolves_to;
        }
    }
}

void PetCollectionView::create_pet_tile(const PetData* data, const PetCollectionEntry& collection_entry) {
    auto& pet_manager = PetManager::get_instance();

    // --- Create a custom tile object ---
    lv_obj_t* tile = lv_obj_create(scrollable_container);
    lv_obj_remove_style_all(tile);
    lv_obj_set_size(tile, LV_PCT(95), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(tile, 5, 0);
    lv_obj_set_style_bg_color(tile, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tile, 5, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    tile_items.push_back(tile);

    // --- Populate the tile ---
    lv_obj_t* icon_area = lv_obj_create(tile);
    lv_obj_remove_style_all(icon_area);
    lv_obj_set_size(icon_area, 32, 32);
    lv_obj_set_style_margin_right(icon_area, 10, 0);

    lv_obj_t* name_label = lv_label_create(tile);
    lv_obj_set_flex_grow(name_label, 1);
    
    if (collection_entry.discovered) {
        lv_obj_t* img = lv_image_create(icon_area);
        lv_image_set_antialias(img, false);
        lv_img_set_zoom(img, 256); // 1x zoom for 32x32 sprites
        lv_obj_center(img);
        
        std::string sprite_path = pet_manager.get_sprite_path_for_id(data->id);
        lv_image_set_src(img, sprite_path.c_str());
        lv_label_set_text_fmt(name_label, "#%04d\n%s", (int)data->id, data->name.c_str());

        if (collection_entry.collected) {
            // Pet line is fully collected: set green background and show checkmark
            lv_obj_set_style_bg_color(tile, lv_palette_lighten(LV_PALETTE_GREEN, 4), 0);
            lv_obj_set_style_image_recolor_opa(img, LV_OPA_TRANSP, 0);
            
            lv_obj_t* check_icon = lv_label_create(tile);
            lv_label_set_text(check_icon, LV_SYMBOL_OK);
            lv_obj_set_style_text_color(check_icon, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else {
            // Discovered but not collected: grayscale sprite
            lv_obj_set_style_image_recolor(img, lv_color_black(), 0);
            lv_obj_set_style_image_recolor_opa(img, LV_OPA_60, 0);
        }
    } else {
        // Undiscovered: show placeholder
        lv_obj_t* question_label = lv_label_create(icon_area);
        lv_label_set_text(question_label, "?");
        lv_obj_set_style_text_font(question_label, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(question_label, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_center(question_label);
        lv_label_set_text_fmt(name_label, "#%04d\n???", (int)data->id);
    }
}

void PetCollectionView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_PRESS_DOWN, nav_up_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_PRESS_DOWN, nav_down_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, back_button_cb, true, this);
}

void PetCollectionView::update_selection() {
    if (tile_items.empty() || selected_index < 0) return;

    // Remove focus style from all items, then add it to the selected one
    for (size_t i = 0; i < tile_items.size(); ++i) {
        lv_obj_remove_style(tile_items[i], &style_focus, LV_STATE_DEFAULT);
    }
    lv_obj_add_style(tile_items[selected_index], &style_focus, LV_STATE_DEFAULT);

    // Scroll the currently selected item into view instantly (no animation)
    lv_obj_scroll_to_view(tile_items[selected_index], LV_ANIM_OFF);
}

void PetCollectionView::on_nav_up() {
    if (tile_items.empty()) {
        return;
    }

    // Implement wrap-around navigation: from first item to last.
    if (selected_index <= 0) {
        selected_index = tile_items.size() - 1;
    } else {
        selected_index--;
    }
    update_selection();
}

void PetCollectionView::on_nav_down() {
    if (tile_items.empty()) {
        return;
    }
    
    // Implement wrap-around navigation: from last item to first.
    if (selected_index >= (int)tile_items.size() - 1) {
        selected_index = 0;
    } else {
        selected_index++;
    }
    update_selection();
}

void PetCollectionView::go_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks ---
void PetCollectionView::nav_up_cb(void* user_data) {
    static_cast<PetCollectionView*>(user_data)->on_nav_up();
}

void PetCollectionView::nav_down_cb(void* user_data) {
    static_cast<PetCollectionView*>(user_data)->on_nav_down();
}

void PetCollectionView::back_button_cb(void* user_data) {
    static_cast<PetCollectionView*>(user_data)->go_back_to_menu();
}