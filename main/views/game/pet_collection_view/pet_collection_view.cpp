#include "pet_collection_view.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "views/view_manager.h"
#include "esp_log.h"
#include "lvgl.h"

static const char* TAG = "PET_COLLECTION_VIEW";
constexpr uint8_t GRID_COLUMNS = 3;

PetCollectionView::PetCollectionView() : selected_index(0) {
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
    lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_ACTIVE);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Pet Encyclopedia");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_top(title, 5, 0);
    lv_obj_set_style_margin_bottom(title, 15, 0);

    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_t* grid = lv_obj_create(parent);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 220, LV_SIZE_CONTENT);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_all(grid, 5, 0);
    lv_obj_set_style_pad_gap(grid, 10, 0);

    auto& pet_manager = PetManager::get_instance();
    auto collection = pet_manager.get_collection();

    uint8_t col = 0, row = 0;
    for (const auto& entry : collection) {
        lv_obj_t* widget = create_pet_widget(grid, entry, col, row);
        pet_widgets.push_back(widget);
        
        col++;
        if (col >= GRID_COLUMNS) { col = 0; row++; }
    }

    update_selection_style();
}

lv_obj_t* PetCollectionView::create_pet_widget(lv_obj_t* parent, const PetCollectionEntry& entry, uint8_t col, uint8_t row) {
    lv_obj_t* widget_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(widget_cont);
    lv_obj_set_size(widget_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(widget_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(widget_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(widget_cont, 5, 0);
    lv_obj_set_style_radius(widget_cont, 8, 0);
    lv_obj_set_style_bg_opa(widget_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(widget_cont, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);

    lv_obj_set_grid_cell(widget_cont, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_CENTER, row, 1);

    lv_obj_t* display_box = lv_image_create(widget_cont);
    lv_obj_set_size(display_box, 48, 48);
    lv_image_set_antialias(display_box, false);
    
    auto& pet_manager = PetManager::get_instance();

    if (entry.collected) {
        lv_obj_set_style_bg_color(widget_cont, lv_palette_main(LV_PALETTE_LIGHT_GREEN), 0);
        PetId final_form_id = pet_manager.get_final_evolution(entry.base_id);
        lv_image_set_src(display_box, pet_manager.get_sprite_path_for_id(final_form_id).c_str());
    } else if (entry.discovered) {
        lv_obj_set_style_bg_color(widget_cont, lv_palette_main(LV_PALETTE_AMBER), 0);
        lv_image_set_src(display_box, pet_manager.get_sprite_path_for_id(entry.base_id).c_str());
    } else {
        lv_image_set_src(display_box, LV_SYMBOL_DIRECTORY);
    }
    
    lv_obj_t* name_label = lv_label_create(widget_cont);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_margin_top(name_label, 5, 0);
    if (entry.discovered || entry.collected) {
        lv_label_set_text(name_label, pet_manager.get_pet_name(entry.base_id).c_str());
        lv_obj_set_style_text_color(name_label, lv_color_black(), 0);
    } else {
        lv_label_set_text_fmt(name_label, "#%03d", (int)entry.base_id);
        lv_obj_set_style_text_color(name_label, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    }

    return widget_cont;
}

void PetCollectionView::update_selection_style() {
    if (pet_widgets.empty()) return;

    for (size_t i = 0; i < pet_widgets.size(); ++i) {
        if (i == selected_index) {
            lv_obj_set_style_outline_width(pet_widgets[i], 3, 0);
            lv_obj_set_style_outline_color(pet_widgets[i], lv_palette_main(LV_PALETTE_BLUE), 0);
            lv_obj_set_style_outline_pad(pet_widgets[i], 3, 0);
        } else {
            lv_obj_set_style_outline_width(pet_widgets[i], 0, 0);
        }
    }
    
    if (selected_index == 0) {
        lv_obj_scroll_to(container, 0, 0, LV_ANIM_ON);
    } else {
        lv_obj_scroll_to_view_recursive(pet_widgets[selected_index], LV_ANIM_ON);
    }
}

void PetCollectionView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, back_button_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, right_press_cb, true, this);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, left_press_cb, true, this);
}

void PetCollectionView::on_left_press() {
    if (pet_widgets.empty()) return;
    selected_index = (selected_index == 0) ? pet_widgets.size() - 1 : selected_index - 1;
    update_selection_style();
}

void PetCollectionView::on_right_press() {
    if (pet_widgets.empty()) return;
    selected_index = (selected_index + 1) % pet_widgets.size();
    update_selection_style();
}

void PetCollectionView::go_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

void PetCollectionView::back_button_cb(void* user_data) {
    static_cast<PetCollectionView*>(user_data)->go_back_to_menu();
}

void PetCollectionView::left_press_cb(void* user_data) {
    static_cast<PetCollectionView*>(user_data)->on_left_press();
}

void PetCollectionView::right_press_cb(void* user_data) {
    static_cast<PetCollectionView*>(user_data)->on_right_press();
}