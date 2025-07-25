#include "habit_history_view.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/habit_data_manager/habit_data_manager.h"
#include "esp_log.h"
#include <string>
#include <time.h> // Required for timestamp formatting

static const char *TAG = "HABIT_HISTORY_VIEW";

// --- Lifecycle Methods ---

HabitHistoryView::HabitHistoryView() {
    ESP_LOGI(TAG, "Constructed");
    current_step = HabitHistoryStep::SELECT_HABIT;
    styles_initialized = false;
}

HabitHistoryView::~HabitHistoryView() {
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

void HabitHistoryView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);

    init_styles();
    setup_ui(container);
    setup_button_handlers();
    
    switch_to_step(HabitHistoryStep::SELECT_HABIT);
}

// --- Style Management ---

void HabitHistoryView::init_styles() {
    if (styles_initialized) return;

    // Style for focused list items
    lv_style_init(&style_list_item_focused);
    lv_style_set_bg_color(&style_list_item_focused, lv_palette_lighten(LV_PALETTE_BLUE, 3));
    lv_style_set_border_color(&style_list_item_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_width(&style_list_item_focused, 2);

    // Style for category headers in the list
    lv_style_init(&style_category_header);
    lv_style_set_text_color(&style_category_header, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_text_font(&style_category_header, &lv_font_montserrat_16);

    styles_initialized = true;
}

void HabitHistoryView::reset_styles() {
    if (!styles_initialized) return;
    lv_style_reset(&style_list_item_focused);
    lv_style_reset(&style_category_header);
    styles_initialized = false;
}


// --- UI Setup & Management ---

void HabitHistoryView::setup_ui(lv_obj_t* parent) {
    create_selection_panel(parent);
    create_history_panel(parent);
}

void HabitHistoryView::create_selection_panel(lv_obj_t* parent) {
    group = lv_group_create();
    lv_group_set_wrap(group, false); // Do not wrap around the list

    panel_select_habit = lv_obj_create(parent);
    lv_obj_remove_style_all(panel_select_habit);
    lv_obj_set_size(panel_select_habit, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel_select_habit, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel_select_habit, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(panel_select_habit, 5, 0);
    lv_obj_set_style_pad_hor(panel_select_habit, 5, 0);
    lv_obj_set_style_pad_ver(panel_select_habit, 10, 0);

    lv_obj_t* title = lv_label_create(panel_select_habit);
    lv_label_set_text(title, "Select a Habit");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    habit_list_container = lv_obj_create(panel_select_habit);
    lv_obj_remove_style_all(habit_list_container);
    lv_obj_set_width(habit_list_container, LV_PCT(100));
    lv_obj_set_flex_grow(habit_list_container, 1);
    lv_obj_set_flex_flow(habit_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(habit_list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(habit_list_container, 5, 0);
}

void HabitHistoryView::create_history_panel(lv_obj_t* parent) {
    panel_show_history = lv_obj_create(parent);
    lv_obj_remove_style_all(panel_show_history);
    lv_obj_set_size(panel_show_history, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel_show_history, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel_show_history, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(panel_show_history, 10, 0);
    lv_obj_set_style_pad_ver(panel_show_history, 10, 0);

    history_title_label = lv_label_create(panel_show_history);
    lv_obj_set_width(history_title_label, LV_PCT(95));
    lv_label_set_long_mode(history_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(history_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(history_title_label, &lv_font_montserrat_20, 0);
    
    history_content_container = lv_obj_create(panel_show_history);
    lv_obj_remove_style_all(history_content_container);
    lv_obj_set_width(history_content_container, LV_PCT(95));
    lv_obj_set_flex_grow(history_content_container, 1); // Make it fill remaining space
    
    lv_obj_t* todo_label = lv_label_create(history_content_container);
    lv_label_set_text(todo_label, "To do: habit history");
    lv_obj_center(todo_label);
}

void HabitHistoryView::switch_to_step(HabitHistoryStep new_step) {
    current_step = new_step;
    lv_obj_add_flag(panel_select_habit, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(panel_show_history, LV_OBJ_FLAG_HIDDEN);
    lv_group_set_default(nullptr);

    if (current_step == HabitHistoryStep::SELECT_HABIT) {
        populate_habit_selector();
        lv_obj_clear_flag(panel_select_habit, LV_OBJ_FLAG_HIDDEN);
        lv_group_set_default(group);
    } else { // SHOW_HISTORY
        on_habit_selected();
        lv_obj_clear_flag(panel_show_history, LV_OBJ_FLAG_HIDDEN);
    }
}

// --- Logic ---

void HabitHistoryView::populate_habit_selector() {
    lv_obj_clean(habit_list_container);
    lv_group_remove_all_objs(group);

    auto all_categories = HabitDataManager::get_active_categories();
    bool habits_found = false;

    for (const auto& category : all_categories) {
        auto habits_in_cat = HabitDataManager::get_active_habits_for_category(category.id);

        if (habits_in_cat.empty()) {
            continue;
        }
        
        habits_found = true;
        
        lv_obj_t* header = lv_label_create(habit_list_container);
        lv_label_set_text(header, category.name.c_str());
        lv_obj_add_style(header, &style_category_header, 0);
        lv_obj_set_width(header, LV_PCT(95));
        lv_obj_set_style_pad_top(header, 10, 0);

        for (const auto& habit : habits_in_cat) {
            lv_obj_t* item = lv_obj_create(habit_list_container);
            lv_obj_remove_style_all(item);
            lv_obj_set_size(item, LV_PCT(95), 40);
            lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(item, 10, 0);
            lv_obj_set_style_radius(item, 5, 0);
            lv_obj_add_style(item, &style_list_item_focused, LV_STATE_FOCUSED);
            
            // Store habit ID in user data
            lv_obj_set_user_data(item, (void*)habit.id);
            
            lv_obj_t* color_indicator = lv_obj_create(item);
            lv_obj_set_size(color_indicator, 10, 25);
            lv_obj_set_style_radius(color_indicator, 3, 0);
            lv_obj_set_style_border_width(color_indicator, 0, 0);
            lv_obj_set_style_bg_color(color_indicator, lv_color_hex(std::stoul(habit.color_hex.substr(1), nullptr, 16)), 0);
            
            lv_obj_t* label = lv_label_create(item);
            lv_label_set_text(label, habit.name.c_str());
            lv_obj_set_flex_grow(label, 1);
            lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);

            lv_group_add_obj(group, item);
        }
    }

    if (!habits_found) {
        lv_obj_t* label = lv_label_create(habit_list_container);
        lv_label_set_text(label, "No active habits found.");
        lv_obj_center(label);
    }
}

void HabitHistoryView::on_habit_selected() {
    Habit* habit = HabitDataManager::get_habit_by_id(this->selected_habit_id);
    if (!habit) {
        ESP_LOGE(TAG, "Cannot show history, habit with ID %lu not found!", this->selected_habit_id);
        // Fallback to selection screen to prevent a crash or empty view
        switch_to_step(HabitHistoryStep::SELECT_HABIT);
        return;
    }
    
    this->selected_habit_name = habit->name;
    lv_label_set_text(history_title_label, selected_habit_name.c_str());
}

// --- Button and Event Handling ---

void HabitHistoryView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press_cb, true, this);
}

void HabitHistoryView::on_ok_press() {
    if (current_step == HabitHistoryStep::SELECT_HABIT) {
        lv_obj_t* focused_obj = lv_group_get_focused(group);
        if (!focused_obj) {
            ESP_LOGW(TAG, "OK pressed but no habit is selected.");
            return;
        }

        this->selected_habit_id = (uint32_t)lv_obj_get_user_data(focused_obj);

        // --- SECTION ADDED FOR LOGGING ---
        ESP_LOGI(TAG, "--- Reading History for Habit ID: %lu ---", this->selected_habit_id);
        
        // 1. Get the history data from the data manager
        HabitHistory history = HabitDataManager::get_history_for_habit(this->selected_habit_id);

        // 2. Check if there are any records
        if (history.completed_dates.empty()) {
            ESP_LOGI(TAG, "No completion records found in LittleFS for this habit.");
        } else {
            ESP_LOGI(TAG, "Found %d completion records:", history.completed_dates.size());
            
            // 3. Iterate and print each record in a human-readable format
            for (const auto& timestamp : history.completed_dates) {
                struct tm timeinfo;
                char buffer[32];
                localtime_r(&timestamp, &timeinfo);
                strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
                ESP_LOGI(TAG, "  - Completed on: %s (Raw Timestamp: %lld)", buffer, (long long)timestamp);
            }
        }
        ESP_LOGI(TAG, "------------------------------------------");
        // --- END OF SECTION ADDED ---

        switch_to_step(HabitHistoryStep::SHOW_HISTORY);
    }
    // No action for OK press on the history screen
}

void HabitHistoryView::on_cancel_press() {
    if (current_step == HabitHistoryStep::SHOW_HISTORY) {
        switch_to_step(HabitHistoryStep::SELECT_HABIT);
    } else {
        view_manager_load_view(VIEW_ID_HABIT_MANAGER);
    }
}

void HabitHistoryView::on_nav_press(bool next) {
    if (current_step == HabitHistoryStep::SELECT_HABIT) {
        if (lv_group_get_obj_count(group) == 0) return;

        if (next) { // Down
            lv_group_focus_next(group);
        } else { // Up
            lv_group_focus_prev(group);
        }
        
        lv_obj_t* focused = lv_group_get_focused(group);
        if (focused) {
            lv_obj_scroll_to_view_recursive(focused, LV_ANIM_ON);
        }
    }
    // No navigation on the history screen
}

// --- Static Callbacks ---
void HabitHistoryView::handle_ok_press_cb(void* user_data) { static_cast<HabitHistoryView*>(user_data)->on_ok_press(); }
void HabitHistoryView::handle_cancel_press_cb(void* user_data) { static_cast<HabitHistoryView*>(user_data)->on_cancel_press(); }
void HabitHistoryView::handle_left_press_cb(void* user_data) { static_cast<HabitHistoryView*>(user_data)->on_nav_press(false); /* Up */ }
void HabitHistoryView::handle_right_press_cb(void* user_data) { static_cast<HabitHistoryView*>(user_data)->on_nav_press(true); /* Down */ }