#include "habit_add_view.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/habit_data_manager/habit_data_manager.h"
#include "esp_log.h"
#include <time.h>
#include "misc/lv_timer.h"

static const char *TAG = "HABIT_ADD_VIEW";

HabitAddView::HabitAddView() {
    ESP_LOGI(TAG, "Constructed");
    preset_colors = {
        "#E6194B", "#3CB44B", "#FFE119", "#4363D8",
        "#F58231", "#911EB4", "#46F0F0", "#F032E6",
        "#BCF60C", "#FABEBE", "#008080", "#E6BEFF"
    };
    if (!preset_colors.empty()) {
        selected_color_hex = preset_colors[0];
    }
    update_habit_name();
}

HabitAddView::~HabitAddView() {
    ESP_LOGI(TAG, "Destructed");
    reset_styles();
    if (group) {
        if (lv_group_get_default() == group) {
            lv_group_set_default(nullptr);
        }
        lv_group_delete(group);
        group = nullptr;
    }
}

void HabitAddView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating UI");
    container = parent;
    
    setup_ui(container);
    setup_button_handlers();
}

void HabitAddView::init_styles() {
    if (styles_initialized) return;

    // --- FIX 1: Make the focused style more prominent with a border ---
    lv_style_init(&style_focused);
    lv_style_set_bg_color(&style_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_focused, lv_color_white());
    lv_style_set_border_width(&style_focused, 2);
    lv_style_set_border_color(&style_focused, lv_color_white());


    lv_style_init(&style_color_cell_focused);
    lv_style_set_border_width(&style_color_cell_focused, 3);
    lv_style_set_border_color(&style_color_cell_focused, lv_color_white());
    lv_style_set_border_opa(&style_color_cell_focused, LV_OPA_100);
    lv_style_set_outline_width(&style_color_cell_focused, 2);
    lv_style_set_outline_color(&style_color_cell_focused, lv_palette_main(LV_PALETTE_GREY));

    styles_initialized = true;
}

void HabitAddView::reset_styles() {
    if (!styles_initialized) return;
    lv_style_reset(&style_focused);
    lv_style_reset(&style_color_cell_focused);
    styles_initialized = false;
}

void HabitAddView::populate_category_roller() {
    auto categories = HabitDataManager::get_active_categories();
    std::string opts_str;
    if (categories.empty()) {
        opts_str = "No Categories\nPlease create one first.";
    } else {
        for (size_t i = 0; i < categories.size(); ++i) {
            opts_str += categories[i].name;
            if (i < categories.size() - 1) {
                opts_str += "\n";
            }
        }
    }
    lv_roller_set_options(category_roller, opts_str.c_str(), LV_ROLLER_MODE_NORMAL);
}

void HabitAddView::create_color_palette(lv_obj_t* parent) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, "Choose a color:");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);

    color_palette_container = lv_obj_create(parent);
    lv_obj_remove_style_all(color_palette_container);
    lv_obj_set_size(color_palette_container, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_layout(color_palette_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(color_palette_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(color_palette_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(color_palette_container, 5, 0);
    lv_obj_set_style_pad_gap(color_palette_container, 8, 0);

    for (const auto& color_str : preset_colors) {
        lv_obj_t* cell = lv_obj_create(color_palette_container);
        lv_obj_set_size(cell, 32, 32);
        lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_style(cell, &style_color_cell_focused, LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(cell, lv_color_hex(std::stoul(color_str.substr(1), nullptr, 16)), 0);
        
        lv_obj_add_event_cb(cell, color_cell_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(cell, color_cell_event_cb, LV_EVENT_FOCUSED, this);
        lv_group_add_obj(group, cell);
    }
}

void HabitAddView::setup_ui(lv_obj_t* parent) {
    init_styles();
    group = lv_group_create();

    // --- FIX 2: Re-enable wrapping to allow navigation from last to first item ---
    lv_group_set_wrap(group, true);
    lv_group_set_focus_cb(group, focus_changed_cb);

    lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_set_style_pad_all(parent, 5, 0); 
    lv_obj_set_style_pad_bottom(parent, 10, 0);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(parent, 15, 0);
    
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Add New Habit");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    
    lv_obj_t* cat_label = lv_label_create(parent);
    lv_label_set_text(cat_label, "Select Category:");
    lv_obj_set_style_text_font(cat_label, &lv_font_montserrat_16, 0);
    category_roller = lv_roller_create(parent);
    lv_obj_set_width(category_roller, LV_PCT(80));
    lv_roller_set_visible_row_count(category_roller, 2);
    populate_category_roller();
    lv_group_add_obj(group, category_roller);
    
    create_color_palette(parent);
    
    lv_obj_t* name_title_label = lv_label_create(parent);
    lv_label_set_text(name_title_label, "Habit Name:");
    lv_obj_set_style_text_font(name_title_label, &lv_font_montserrat_16, 0);

    name_label = lv_label_create(parent);
    lv_obj_set_width(name_label, LV_PCT(90));
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(name_label, current_habit_name.c_str());
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);

    refresh_name_button = lv_button_create(parent);
    lv_obj_set_width(refresh_name_button, LV_PCT(80));
    lv_obj_add_style(refresh_name_button, &style_focused, LV_STATE_FOCUSED);
    lv_obj_t* refresh_btn_label = lv_label_create(refresh_name_button);
    lv_label_set_text(refresh_btn_label, "Refresh Name");
    lv_obj_center(refresh_btn_label);
    lv_group_add_obj(group, refresh_name_button);

    create_button = lv_button_create(parent);
    lv_obj_set_width(create_button, LV_PCT(80));
    lv_obj_add_style(create_button, &style_focused, LV_STATE_FOCUSED);
    lv_obj_t* btn_label = lv_label_create(create_button);
    lv_label_set_text(btn_label, "Create Habit");
    lv_obj_center(btn_label);
    lv_group_add_obj(group, create_button);

    lv_group_set_default(group);

    // --- FIX 3: Manually set the initial scroll position to the top ---
    lv_obj_scroll_to(parent, 0, 0, LV_ANIM_OFF);
}


void HabitAddView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press_cb, true, this);
}

void HabitAddView::update_habit_name() {
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char name_buffer[64];
    strftime(name_buffer, sizeof(name_buffer), "Habit_%y%m%d_%H%M%S", &timeinfo);
    current_habit_name = name_buffer;

    if (name_label) {
        lv_label_set_text(name_label, current_habit_name.c_str());
    }
}

void HabitAddView::on_ok_press() {
    lv_obj_t* focused_obj = lv_group_get_focused(group);
    if (!focused_obj) return;

    if (focused_obj == refresh_name_button) {
        update_habit_name();
        ESP_LOGI(TAG, "Habit name refreshed to: %s", current_habit_name.c_str());

    } else if (focused_obj == create_button) {
        auto categories = HabitDataManager::get_active_categories();
        if (categories.empty()) {
            ESP_LOGE(TAG, "Cannot create habit, no categories exist.");
            return;
        }
        uint16_t selected_idx = lv_roller_get_selected(category_roller);
        if (selected_idx >= categories.size()) {
            ESP_LOGE(TAG, "Invalid category index selected.");
            return;
        }
        uint32_t category_id = categories[selected_idx].id;
        
        ESP_LOGI(TAG, "Creating habit: Name='%s', CategoryID=%lu, Color=%s", 
            current_habit_name.c_str(), category_id, selected_color_hex.c_str());

        HabitDataManager::add_habit(current_habit_name, category_id, selected_color_hex);
        
        lv_timer_create(show_creation_toast_cb, 500, this);
        
    } else {
        lv_obj_send_event(focused_obj, LV_EVENT_CLICKED, nullptr);
    }
}

void HabitAddView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed, returning to habit menu.");
    view_manager_load_view(VIEW_ID_HABIT_MANAGER);
}

void HabitAddView::on_nav_press(bool next) {
    if (next) {
        lv_group_focus_next(group);
    } else {
        lv_group_focus_prev(group);
    }
}

void HabitAddView::color_cell_event_cb(lv_event_t * e) {
    HabitAddView* view = static_cast<HabitAddView*>(lv_event_get_user_data(e));
    lv_obj_t* cell = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        uint32_t cell_idx = lv_obj_get_index(cell);
        if (cell_idx < view->preset_colors.size()) {
            view->selected_color_hex = view->preset_colors[cell_idx];
            ESP_LOGD(TAG, "Color selected: %s", view->selected_color_hex.c_str());
        }
    }
}

void HabitAddView::show_creation_toast_cb(lv_timer_t * timer) {
    view_manager_load_view(VIEW_ID_HABIT_MANAGER);
    lv_timer_delete(timer);
}

void HabitAddView::focus_changed_cb(lv_group_t* group) {
    lv_obj_t* focused_obj = lv_group_get_focused(group);
    if (focused_obj) {
        lv_obj_scroll_to_view(focused_obj, LV_ANIM_ON);
    }
}

// Static Callbacks
void HabitAddView::handle_ok_press_cb(void* user_data) { static_cast<HabitAddView*>(user_data)->on_ok_press(); }
void HabitAddView::handle_cancel_press_cb(void* user_data) { static_cast<HabitAddView*>(user_data)->on_cancel_press(); }
void HabitAddView::handle_left_press_cb(void* user_data) { static_cast<HabitAddView*>(user_data)->on_nav_press(false); }
void HabitAddView::handle_right_press_cb(void* user_data) { static_cast<HabitAddView*>(user_data)->on_nav_press(true); }