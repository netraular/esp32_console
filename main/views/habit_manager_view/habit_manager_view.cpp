#include "habit_manager_view.h"
#include "components/status_bar_component/status_bar_component.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"

static const char *TAG = "HABIT_MANAGER_VIEW";

HabitManagerView::HabitManagerView() {
    ESP_LOGI(TAG, "Constructed");
}

HabitManagerView::~HabitManagerView() {
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

void HabitManagerView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);

    setup_ui(container);
    setup_button_handlers();
}

void HabitManagerView::setup_ui(lv_obj_t* parent) {
    init_styles();

    group = lv_group_create();
    lv_group_set_wrap(group, true);

    lv_obj_t* content_container = lv_obj_create(parent);
    lv_obj_remove_style_all(content_container);
    lv_obj_set_size(content_container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(content_container);
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content_container, 20, 0);

    lv_obj_t* title = lv_label_create(content_container);
    lv_label_set_text(title, "Habit Tracker");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    list_menu = lv_list_create(content_container);
    lv_obj_set_size(list_menu, LV_PCT(90), LV_SIZE_CONTENT);

    lv_obj_t* btn;
    
    btn = lv_list_add_button(list_menu, LV_SYMBOL_PLAY, "Track Today's Habits");
    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(group, btn);

    btn = lv_list_add_button(list_menu, LV_SYMBOL_EDIT, "Manage Categories");
    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(group, btn);

    btn = lv_list_add_button(list_menu, LV_SYMBOL_LIST, "Manage Habits");
    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(group, btn);

    btn = lv_list_add_button(list_menu, LV_SYMBOL_EYE_OPEN, "View History");
    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(group, btn);

    lv_group_set_default(group);
}

void HabitManagerView::init_styles() {
    if (styles_initialized) return;
    lv_style_init(&style_focused);
    lv_style_set_bg_color(&style_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_focused, lv_color_white());
    styles_initialized = true;
}

void HabitManagerView::reset_styles() {
    if (!styles_initialized) return;
    lv_style_reset(&style_focused);
    styles_initialized = false;
}

void HabitManagerView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press_cb, true, this);
}

void HabitManagerView::on_nav_press(bool next) {
    if (group) {
        if (next) {
            lv_group_focus_next(group);
        } else {
            lv_group_focus_prev(group);
        }
        lv_obj_t* focused = lv_group_get_focused(group);
        if (focused) {
            lv_obj_scroll_to_view(focused, LV_ANIM_ON);
        }
    }
}

void HabitManagerView::on_ok_press() {
    lv_obj_t* focused_btn = lv_group_get_focused(group);
    if (!focused_btn) return;

    uint32_t index = lv_obj_get_index(focused_btn);
    ESP_LOGI(TAG, "OK pressed on list item index %lu", index);

    switch (index) {
        case 0: // Track Today's Habits
            ESP_LOGI(TAG, "Navigate to: Track Today's Habits");
            view_manager_load_view(VIEW_ID_TRACK_HABITS);
            break;
        case 1: // Manage Categories
            ESP_LOGI(TAG, "Navigate to: Manage Categories");
            view_manager_load_view(VIEW_ID_HABIT_CATEGORY_MANAGER);
            break;
        case 2: // Manage Habits -> NOW Add Habit
            ESP_LOGI(TAG, "Navigate to: Add New Habit");
            view_manager_load_view(VIEW_ID_HABIT_ADD);
            break;
        case 3: // View History
            ESP_LOGI(TAG, "Navigate to: View History (Not Implemented)");
            break;
    }
}

void HabitManagerView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed, returning to main menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks ---
void HabitManagerView::handle_ok_press_cb(void* user_data) {
    static_cast<HabitManagerView*>(user_data)->on_ok_press();
}

void HabitManagerView::handle_cancel_press_cb(void* user_data) {
    static_cast<HabitManagerView*>(user_data)->on_cancel_press();
}

void HabitManagerView::handle_left_press_cb(void* user_data) {
    static_cast<HabitManagerView*>(user_data)->on_nav_press(false);
}

void HabitManagerView::handle_right_press_cb(void* user_data) {
    static_cast<HabitManagerView*>(user_data)->on_nav_press(true);
}