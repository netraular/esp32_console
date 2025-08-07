#include "pet_collection_view.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "views/view_manager.h"
#include "esp_log.h"
#include "lvgl.h"

static const char* TAG = "PET_COLLECTION_VIEW";

PetCollectionView::PetCollectionView() {
    ESP_LOGI(TAG, "PetCollectionView constructed");
}

PetCollectionView::~PetCollectionView() {
    ESP_LOGI(TAG, "PetCollectionView destructed");
}

void PetCollectionView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);
    
    setup_ui(container);
    setup_button_handlers();
}

void PetCollectionView::setup_ui(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Pet Collection");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_bottom(title, 15, 0);

    // --- Create the Grid Layout ---
    // Define the grid structure: 2 columns, with each column taking up 1 fraction of the space.
    // Rows will be created automatically based on content size.
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_t* grid = lv_obj_create(parent);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 220, LV_SIZE_CONTENT); // Fixed width, height adjusts to content
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_row(grid, 10, 0);
    lv_obj_set_style_pad_column(grid, 10, 0);

    // --- Populate the Grid ---
    auto& pet_manager = PetManager::get_instance();
    auto collection = pet_manager.get_collection();

    for (const auto& entry : collection) {
        create_pet_widget(grid, entry);
    }
}

void PetCollectionView::create_pet_widget(lv_obj_t* parent_grid, const PetCollectionEntry& entry) {
    lv_obj_t* widget_cont = lv_obj_create(parent_grid);
    lv_obj_remove_style_all(widget_cont);
    lv_obj_set_size(widget_cont, LV_PCT(100), LV_SIZE_CONTENT); // Take full cell width
    lv_obj_set_flex_flow(widget_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(widget_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(widget_cont, 5, 0);
    lv_obj_set_style_radius(widget_cont, 8, 0);
    lv_obj_set_style_border_width(widget_cont, 1, 0);
    lv_obj_set_style_border_color(widget_cont, lv_palette_main(LV_PALETTE_GREY), 0);

    // The main display object (square)
    lv_obj_t* display_box = lv_obj_create(widget_cont);
    lv_obj_set_size(display_box, 60, 60);
    
    // Label for the content inside the box ('?' or icon)
    lv_obj_t* content_label = lv_label_create(display_box);
    lv_obj_set_style_text_font(content_label, &lv_font_montserrat_24, 0);
    lv_obj_center(content_label);
    
    // Get the base name from the manager
    std::string pet_name = PetManager::get_instance().get_pet_base_name(entry.type);

    // --- Apply Styling Based on Collection Status ---
    if (entry.collected) {
        // Collected: Full color and pet's base name
        lv_label_set_text(content_label, pet_name.c_str());
        lv_obj_set_style_bg_color(display_box, lv_palette_main(LV_PALETTE_LIGHT_GREEN), 0);
        lv_obj_set_style_text_color(content_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(content_label, &lv_font_montserrat_12, 0);
    } else if (entry.discovered) {
        // Discovered but not collected: Black icon
        lv_label_set_text(content_label, pet_name.c_str());
        lv_obj_set_style_bg_color(display_box, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
        lv_obj_set_style_text_color(content_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(content_label, &lv_font_montserrat_12, 0);
    } else {
        // Not discovered: '?'
        lv_label_set_text(content_label, "?");
        lv_obj_set_style_bg_color(display_box, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
        lv_obj_set_style_text_color(content_label, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    }
}

void PetCollectionView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, back_button_cb, true, this);
}

void PetCollectionView::go_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

void PetCollectionView::back_button_cb(void* user_data) {
    static_cast<PetCollectionView*>(user_data)->go_back_to_menu();
}