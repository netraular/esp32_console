#include "habit_add_view.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/habit_data_manager/habit_data_manager.h"
#include "esp_log.h"
#include <time.h>
#include <cstring>
#include "misc/lv_timer.h"

static const char *TAG = "HABIT_ADD_VIEW";

// --- Lifecycle Methods ---

HabitAddView::HabitAddView() {
    ESP_LOGI(TAG, "Constructed");
    preset_colors = {
        "#E6194B", "#3CB44B", "#FFE119", "#4363D8",
        "#F58231", "#911EB4", "#46F0F0", "#F032E6",
        "#BCF60C", "#FABEBE", "#008080", "#E6BEFF"
    };
    current_habit_name = ""; // Start with an empty name
    color_is_selected = false;
    init_styles();
}

HabitAddView::~HabitAddView() {
    ESP_LOGI(TAG, "Destructed");
    if (color_panel_group) {
        if (lv_group_get_default() == color_panel_group) lv_group_set_default(nullptr);
        lv_group_delete(color_panel_group);
        color_panel_group = nullptr;
    }
    if (name_panel_group) {
        if (lv_group_get_default() == name_panel_group) lv_group_set_default(nullptr);
        lv_group_delete(name_panel_group);
        name_panel_group = nullptr;
    }

    reset_styles();
}

void HabitAddView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);

    setup_ui(container);
    setup_button_handlers();
}

void HabitAddView::init_styles() {
    if (styles_initialized) return;
    
    // Style for focused buttons (like 'Generate Name' and 'Next')
    lv_style_init(&style_button_focused);
    lv_style_set_bg_color(&style_button_focused, lv_palette_lighten(LV_PALETTE_BLUE, 2));
    lv_style_set_border_color(&style_button_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_width(&style_button_focused, 2);

    // Style for focused color cells (blue border)
    lv_style_init(&style_color_cell_focused);
    lv_style_set_border_width(&style_color_cell_focused, 2);
    lv_style_set_border_color(&style_color_cell_focused, lv_palette_main(LV_PALETTE_BLUE));

    // Style for CHECKED/selected color cells (thicker white border)
    lv_style_init(&style_color_cell_checked);
    lv_style_set_border_width(&style_color_cell_checked, 4);
    lv_style_set_border_color(&style_color_cell_checked, lv_color_white());
    lv_style_set_border_opa(&style_color_cell_checked, LV_OPA_COVER);
    
    styles_initialized = true;
}

void HabitAddView::reset_styles() {
    if (!styles_initialized) return;
    lv_style_reset(&style_button_focused);
    lv_style_reset(&style_color_cell_focused);
    lv_style_reset(&style_color_cell_checked);
    styles_initialized = false;
}

// --- UI Panel Creation ---

void HabitAddView::setup_ui(lv_obj_t* parent) {
    create_category_panel(parent);
    create_name_panel(parent);
    create_color_create_panel(parent);
    switch_to_step(HabitAddStep::STEP_CATEGORY);
}

void HabitAddView::create_category_panel(lv_obj_t* parent) {
    panel_category = lv_obj_create(parent);
    lv_obj_remove_style_all(panel_category);
    lv_obj_set_size(panel_category, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel_category, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel_category, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(panel_category, 20, 0);

    lv_obj_t* title = lv_label_create(panel_category);
    lv_label_set_text(title, "1. Select Category");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);

    category_roller = lv_roller_create(panel_category);
    lv_obj_set_width(category_roller, LV_PCT(85));
    lv_roller_set_visible_row_count(category_roller, 4);
    populate_category_roller();
    lv_obj_set_style_bg_opa(category_roller, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_text_color(category_roller, lv_palette_main(LV_PALETTE_BLUE), LV_PART_SELECTED);
}

void HabitAddView::create_name_panel(lv_obj_t* parent) {
    panel_name = lv_obj_create(parent);
    lv_obj_remove_style_all(panel_name);
    lv_obj_set_size(panel_name, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel_name, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel_name, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(panel_name, 20, 0);

    name_panel_group = lv_group_create();
    lv_group_set_wrap(name_panel_group, true);

    lv_obj_t* title = lv_label_create(panel_name);
    lv_label_set_text(title, "2. Set Name");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);

    name_label = lv_label_create(panel_name);
    lv_obj_set_width(name_label, LV_PCT(90));
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(name_label, "Press 'Generate Name'");
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(name_label, lv_palette_main(LV_PALETTE_GREY), 0);

    lv_obj_t* btn_container = lv_obj_create(panel_name);
    lv_obj_remove_style_all(btn_container);
    lv_obj_set_width(btn_container, LV_PCT(95));
    lv_obj_set_height(btn_container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    btn_generate_name = lv_button_create(btn_container);
    lv_obj_set_size(btn_generate_name, 100, LV_SIZE_CONTENT);
    lv_obj_t* gen_label = lv_label_create(btn_generate_name);
    lv_label_set_text(gen_label, "Generate\nName");
    lv_obj_center(gen_label);
    lv_obj_add_style(btn_generate_name, &style_button_focused, LV_STATE_FOCUSED); // Apply focus style
    lv_group_add_obj(name_panel_group, btn_generate_name);

    btn_next_name = lv_button_create(btn_container);
    lv_obj_set_size(btn_next_name, 100, LV_SIZE_CONTENT);
    lv_obj_t* next_label = lv_label_create(btn_next_name);
    lv_label_set_text(next_label, "Next");
    lv_obj_center(next_label);
    lv_obj_add_state(btn_next_name, LV_STATE_DISABLED);
    lv_obj_add_style(btn_next_name, &style_button_focused, LV_STATE_FOCUSED); // Apply focus style
    lv_group_add_obj(name_panel_group, btn_next_name);
}

void HabitAddView::create_color_create_panel(lv_obj_t* parent) {
    panel_color_create = lv_obj_create(parent);
    lv_obj_remove_style_all(panel_color_create);
    lv_obj_set_size(panel_color_create, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel_color_create, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel_color_create, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(panel_color_create, 15, 0);
    lv_obj_set_style_pad_top(panel_color_create, 15, 0);


    color_panel_group = lv_group_create();
    lv_group_set_wrap(color_panel_group, true);

    lv_obj_t* title = lv_label_create(panel_color_create);
    lv_label_set_text(title, "3. Choose Color & Create");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);

    lv_obj_t* color_palette_container = lv_obj_create(panel_color_create);
    lv_obj_remove_style_all(color_palette_container);
    // --- MODIFICATION START ---
    // Set a fixed width that can hold 4 cells with padding.
    // 4 cells * 35px width + 3 gaps * 10px = 140 + 30 = 170. Add some padding.
    lv_obj_set_width(color_palette_container, 180);
    lv_obj_set_height(color_palette_container, LV_SIZE_CONTENT);
    // --- MODIFICATION END ---
    lv_obj_set_layout(color_palette_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(color_palette_container, LV_FLEX_FLOW_ROW_WRAP);
    // Use LV_FLEX_ALIGN_CENTER to center the items within the container.
    lv_obj_set_flex_align(color_palette_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(color_palette_container, 5, 0);
    lv_obj_set_style_pad_gap(color_palette_container, 10, 0);

    for (const auto& color_str : preset_colors) {
        lv_obj_t* cell = lv_obj_create(color_palette_container);
        lv_obj_set_size(cell, 35, 35);
        lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(cell, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(cell, lv_color_hex(std::stoul(color_str.substr(1), nullptr, 16)), 0);
        lv_obj_add_style(cell, &style_color_cell_focused, LV_STATE_FOCUSED);
        lv_obj_add_style(cell, &style_color_cell_checked, LV_STATE_CHECKED); // Apply checked style
        lv_group_add_obj(color_panel_group, cell);
    }

    btn_create_habit = lv_button_create(panel_color_create);
    lv_obj_set_width(btn_create_habit, LV_PCT(80));
    lv_obj_t* create_label = lv_label_create(btn_create_habit);
    lv_label_set_text(create_label, "Create Habit");
    lv_obj_center(create_label);
    lv_obj_add_state(btn_create_habit, LV_STATE_DISABLED); // Disabled until a color is selected
    lv_obj_add_style(btn_create_habit, &style_button_focused, LV_STATE_FOCUSED); // Apply focus style
    lv_group_add_obj(color_panel_group, btn_create_habit);
}

// --- State and Logic ---

void HabitAddView::switch_to_step(HabitAddStep new_step) {
    current_step = new_step;
    lv_obj_add_flag(panel_category, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(panel_name, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(panel_color_create, LV_OBJ_FLAG_HIDDEN);
    lv_group_set_default(nullptr);

    switch (current_step) {
        case HabitAddStep::STEP_CATEGORY:
            lv_obj_clear_flag(panel_category, LV_OBJ_FLAG_HIDDEN);
            break;
        case HabitAddStep::STEP_NAME:
            lv_obj_clear_flag(panel_name, LV_OBJ_FLAG_HIDDEN);
            lv_group_set_default(name_panel_group);
            lv_group_focus_obj(btn_generate_name);
            break;
        case HabitAddStep::STEP_COLOR_CREATE:
            lv_obj_clear_flag(panel_color_create, LV_OBJ_FLAG_HIDDEN);
            lv_group_set_default(color_panel_group);
            if (lv_group_get_obj_count(color_panel_group) > 0) {
                 lv_obj_t* color_container = lv_obj_get_child(panel_color_create, 1);
                 lv_group_focus_obj(lv_obj_get_child(color_container, 0));
            }
            break;
    }
}

void HabitAddView::populate_category_roller() {
    auto categories = HabitDataManager::get_active_categories();
    std::string opts_str;
    if (categories.empty()) {
        opts_str = "No Categories\nCreate one first";
        lv_obj_add_state(category_roller, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(category_roller, LV_STATE_DISABLED);
        for (size_t i = 0; i < categories.size(); ++i) {
            opts_str += categories[i].name;
            if (i < categories.size() - 1) opts_str += "\n";
        }
    }
    lv_roller_set_options(category_roller, opts_str.c_str(), LV_ROLLER_MODE_NORMAL);
}

void HabitAddView::update_habit_name() {
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char name_buffer[64];
    strftime(name_buffer, sizeof(name_buffer), "Habit_%H%M%S", &timeinfo);
    current_habit_name = name_buffer;
    if (name_label) {
        lv_label_set_text(name_label, current_habit_name.c_str());
        lv_obj_set_style_text_color(name_label, lv_color_black(), 0);
    }
    if (btn_next_name && lv_obj_has_state(btn_next_name, LV_STATE_DISABLED)) {
        lv_obj_clear_state(btn_next_name, LV_STATE_DISABLED);
    }
}

void HabitAddView::handle_create_habit() {
    if (!color_is_selected) {
        ESP_LOGW(TAG, "Create button pressed but no color selected.");
        return;
    }
    ESP_LOGI(TAG, "Creating habit: Name='%s', CategoryID=%lu, Color=%s",
        current_habit_name.c_str(), selected_category_id, selected_color_hex.c_str());

    if(HabitDataManager::add_habit(current_habit_name, selected_category_id, selected_color_hex)) {
        show_creation_toast();
    } else {
        ESP_LOGE(TAG, "Failed to add habit via data manager.");
    }
}

void HabitAddView::show_creation_toast() {
    button_manager_unregister_view_handlers();
    lv_obj_t* toast = lv_label_create(lv_screen_active());
    lv_obj_set_style_bg_color(toast, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_text_color(toast, lv_color_white(), 0);
    lv_obj_set_style_pad_all(toast, 10, 0);
    lv_obj_set_style_radius(toast, 5, 0);
    lv_label_set_text(toast, "Habit Created!");
    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_timer_t* timer = lv_timer_create(return_to_manager_cb, 1500, nullptr);
    lv_timer_set_repeat_count(timer, 1);
}

void HabitAddView::go_to_next_step() {
    if (current_step == HabitAddStep::STEP_CATEGORY) {
        auto categories = HabitDataManager::get_active_categories();
        if (categories.empty()) {
             ESP_LOGE(TAG, "Cannot proceed, no categories available!");
             return;
        }
        uint16_t selected_idx = lv_roller_get_selected(category_roller);
        selected_category_id = categories[selected_idx].id;
    }
    if (current_step < HabitAddStep::STEP_COLOR_CREATE) {
        switch_to_step(static_cast<HabitAddStep>(static_cast<int>(current_step) + 1));
    }
}

void HabitAddView::go_to_previous_step() {
    if (current_step > HabitAddStep::STEP_CATEGORY) {
        switch_to_step(static_cast<HabitAddStep>(static_cast<int>(current_step) - 1));
    }
}

// --- Event and Button Handlers ---

void HabitAddView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press_cb, true, this);
}

void HabitAddView::on_ok_press() {
    switch (current_step) {
        case HabitAddStep::STEP_CATEGORY:
            go_to_next_step();
            break;
        case HabitAddStep::STEP_NAME: {
            lv_obj_t* focused = lv_group_get_focused(name_panel_group);
            if (focused == btn_generate_name) {
                update_habit_name();
            } else if (focused == btn_next_name && !lv_obj_has_state(btn_next_name, LV_STATE_DISABLED)) {
                go_to_next_step();
            }
            break;
        }
        case HabitAddStep::STEP_COLOR_CREATE: {
            lv_obj_t* focused = lv_group_get_focused(color_panel_group);
            if (focused == btn_create_habit) {
                handle_create_habit();
            } else { // It's a color cell
                // Mark this color as checked and uncheck others
                lv_obj_t* color_container = lv_obj_get_parent(focused);
                for(uint32_t i = 0; i < lv_obj_get_child_count(color_container); i++) {
                    lv_obj_clear_state(lv_obj_get_child(color_container, i), LV_STATE_CHECKED);
                }
                lv_obj_add_state(focused, LV_STATE_CHECKED);
                
                uint32_t color_index = lv_obj_get_index(focused);
                if (color_index < preset_colors.size()) {
                    selected_color_hex = preset_colors[color_index];
                    color_is_selected = true;
                    ESP_LOGI(TAG, "Color %s selected.", selected_color_hex.c_str());

                    // Enable and focus the create button
                    lv_obj_clear_state(btn_create_habit, LV_STATE_DISABLED);
                    lv_group_focus_obj(btn_create_habit);
                }
            }
            break;
        }
    }
}

void HabitAddView::on_cancel_press() {
    if (current_step == HabitAddStep::STEP_COLOR_CREATE) {
        lv_obj_t* focused = lv_group_get_focused(color_panel_group);
        if (focused == btn_create_habit && color_is_selected) {
            // Find the checked cell and focus it
            lv_obj_t* color_container = lv_obj_get_child(panel_color_create, 1);
            for(uint32_t i = 0; i < lv_obj_get_child_count(color_container); i++) {
                lv_obj_t* cell = lv_obj_get_child(color_container, i);
                if (lv_obj_has_state(cell, LV_STATE_CHECKED)) {
                    lv_group_focus_obj(cell);
                    return; // Action handled, don't go back a step
                }
            }
        }
    }

    if (current_step > HabitAddStep::STEP_CATEGORY) {
        go_to_previous_step();
    } else {
        ESP_LOGI(TAG, "Cancel pressed on first step, returning to habit menu.");
        view_manager_load_view(VIEW_ID_HABIT_MANAGER);
    }
}

void HabitAddView::on_left_press() {
    switch(current_step) {
        case HabitAddStep::STEP_CATEGORY:
             lv_roller_set_selected(category_roller, (lv_roller_get_selected(category_roller) - 1 + lv_roller_get_option_count(category_roller)) % lv_roller_get_option_count(category_roller), LV_ANIM_ON);
            break;
        case HabitAddStep::STEP_NAME:
            lv_group_focus_prev(name_panel_group);
            break;
        case HabitAddStep::STEP_COLOR_CREATE:
            lv_group_focus_prev(color_panel_group);
            break;
    }
}

void HabitAddView::on_right_press() {
    switch(current_step) {
        case HabitAddStep::STEP_CATEGORY:
            lv_roller_set_selected(category_roller, (lv_roller_get_selected(category_roller) + 1) % lv_roller_get_option_count(category_roller), LV_ANIM_ON);
            break;
        case HabitAddStep::STEP_NAME:
            lv_group_focus_next(name_panel_group);
            break;
        case HabitAddStep::STEP_COLOR_CREATE:
            lv_group_focus_next(color_panel_group);
            break;
    }
}


// --- Static Callbacks ---
void HabitAddView::return_to_manager_cb(lv_timer_t * timer) {
    view_manager_load_view(VIEW_ID_HABIT_MANAGER);
}

void HabitAddView::handle_ok_press_cb(void* user_data) { static_cast<HabitAddView*>(user_data)->on_ok_press(); }
void HabitAddView::handle_cancel_press_cb(void* user_data) { static_cast<HabitAddView*>(user_data)->on_cancel_press(); }
void HabitAddView::handle_left_press_cb(void* user_data) { static_cast<HabitAddView*>(user_data)->on_left_press(); }
void HabitAddView::handle_right_press_cb(void* user_data) { static_cast<HabitAddView*>(user_data)->on_right_press(); }