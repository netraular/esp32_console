#include "pet_collection_view.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "views/view_manager.h"
#include "esp_log.h"
#include "lvgl.h"

static const char* TAG = "PET_COLLECTION_VIEW";
constexpr uint8_t GRID_COLUMNS = 2;

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

    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_t* grid = lv_obj_create(parent);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 220, LV_SIZE_CONTENT);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_row(grid, 10, 0);
    lv_obj_set_style_pad_column(grid, 10, 0);

    auto& pet_manager = PetManager::get_instance();
    auto collection = pet_manager.get_collection();

    uint8_t col = 0;
    uint8_t row = 0;
    for (const auto& entry : collection) {
        // Create the widget and store its pointer
        lv_obj_t* widget = create_pet_widget(grid, entry, col, row);
        pet_widgets.push_back(widget);
        
        col++;
        if (col >= GRID_COLUMNS) {
            col = 0;
            row++;
        }
    }

    // Apply the initial selection style
    update_selection_style();
}

lv_obj_t* PetCollectionView::create_pet_widget(lv_obj_t* parent_grid, const PetCollectionEntry& entry, uint8_t col, uint8_t row) {
    lv_obj_t* widget_cont = lv_obj_create(parent_grid);
    lv_obj_remove_style_all(widget_cont);
    lv_obj_set_size(widget_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(widget_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(widget_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(widget_cont, 5, 0);
    lv_obj_set_style_radius(widget_cont, 8, 0);
    
    lv_obj_set_grid_cell(widget_cont, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_CENTER, row, 1);

    lv_obj_t* display_box = lv_obj_create(widget_cont);
    lv_obj_set_size(display_box, 60, 60);
    
    lv_obj_t* content_label = lv_label_create(display_box);
    lv_obj_set_style_text_font(content_label, &lv_font_montserrat_24, 0);
    lv_obj_center(content_label);
    
    std::string pet_name = PetManager::get_instance().get_pet_base_name(entry.type);

    if (entry.collected) {
        lv_label_set_text(content_label, pet_name.c_str());
        lv_obj_set_style_bg_color(display_box, lv_palette_main(LV_PALETTE_LIGHT_GREEN), 0);
        lv_obj_set_style_text_color(content_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(content_label, &lv_font_montserrat_12, 0);
    } else if (entry.discovered) {
        lv_label_set_text(content_label, pet_name.c_str());
        lv_obj_set_style_bg_color(display_box, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
        lv_obj_set_style_text_color(content_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(content_label, &lv_font_montserrat_12, 0);
    } else {
        lv_label_set_text(content_label, "?");
        lv_obj_set_style_bg_color(display_box, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
        lv_obj_set_style_text_color(content_label, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    }
    
    return widget_cont; // Return the pointer to the main container
}

void PetCollectionView::update_selection_style() {
    if (pet_widgets.empty()) return;

    for (size_t i = 0; i < pet_widgets.size(); ++i) {
        lv_obj_t* widget = pet_widgets[i];
        if (i == selected_index) {
            // Apply selected style: thick, colored border
            lv_obj_set_style_border_width(widget, 3, 0);
            lv_obj_set_style_border_color(widget, lv_palette_main(LV_PALETTE_BLUE), 0);
        } else {
            // Apply default style: thin, grey border
            lv_obj_set_style_border_width(widget, 1, 0);
            lv_obj_set_style_border_color(widget, lv_palette_main(LV_PALETTE_GREY), 0);
        }
    }
}

void PetCollectionView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, back_button_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, right_press_cb, true, this);
}

void PetCollectionView::on_left_press() {
    if (pet_widgets.empty()) return;

    selected_index--;
    if (selected_index < 0) {
        selected_index = pet_widgets.size() - 1; // Wrap around to the end
    }
    update_selection_style();
}

void PetCollectionView::on_right_press() {
    if (pet_widgets.empty()) return;

    selected_index++;
    if (selected_index >= pet_widgets.size()) {
        selected_index = 0; // Wrap around to the beginning
    }
    update_selection_style();
}

void PetCollectionView::go_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks ---

void PetCollectionView::back_button_cb(void* user_data) {
    static_cast<PetCollectionView*>(user_data)->go_back_to_menu();
}

void PetCollectionView::left_press_cb(void* user_data) {
    static_cast<PetCollectionView*>(user_data)->on_left_press();
}

void PetCollectionView::right_press_cb(void* user_data) {
    static_cast<PetCollectionView*>(user_data)->on_right_press();
}