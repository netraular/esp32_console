#include "habit_history_view.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/habit_data_manager/habit_data_manager.h"
#include "esp_log.h"
#include <string>
#include <algorithm> // For std::sort and std::binary_search
#include <time.h> // Required for timestamp formatting

static const char *TAG = "HABIT_HISTORY_VIEW";

// --- Static Helper Functions ---
/**
 * @brief Checks if two time_t values fall on the same calendar day.
 * @param t1 First timestamp.
 * @param t2 Second timestamp.
 * @return True if they are on the same day, false otherwise.
 */
static bool is_same_day(time_t t1, time_t t2) {
    struct tm tm1, tm2;
    localtime_r(&t1, &tm1);
    localtime_r(&t2, &tm2);
    return (tm1.tm_year == tm2.tm_year &&
            tm1.tm_mon == tm2.tm_mon &&
            tm1.tm_mday == tm2.tm_mday);
}


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

    // Style for the calendar grid cells
    lv_style_init(&style_calendar_cell);
    lv_style_set_radius(&style_calendar_cell, 2);
    lv_style_set_bg_color(&style_calendar_cell, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_style_set_bg_opa(&style_calendar_cell, LV_OPA_COVER);
    lv_style_set_border_width(&style_calendar_cell, 0);

    // Style for the current day's cell
    lv_style_init(&style_calendar_cell_today);
    lv_style_set_border_width(&style_calendar_cell_today, 2);
    lv_style_set_border_color(&style_calendar_cell_today, lv_palette_main(LV_PALETTE_RED));

    styles_initialized = true;
}

void HabitHistoryView::reset_styles() {
    if (!styles_initialized) return;
    lv_style_reset(&style_list_item_focused);
    lv_style_reset(&style_category_header);
    lv_style_reset(&style_calendar_cell);
    lv_style_reset(&style_calendar_cell_today);
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
    lv_obj_set_flex_align(panel_show_history, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(panel_show_history, 10, 0);
    lv_obj_set_style_pad_hor(panel_show_history, 5, 0);

    // --- New Title Container (Flex Row) ---
    lv_obj_t* title_container = lv_obj_create(panel_show_history);
    lv_obj_remove_style_all(title_container);
    lv_obj_set_width(title_container, LV_PCT(100));
    lv_obj_set_height(title_container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(title_container, 10, 0);

    // --- Color Indicator Circle ---
    history_color_indicator = lv_obj_create(title_container);
    lv_obj_remove_style_all(history_color_indicator);
    lv_obj_set_size(history_color_indicator, 20, 20);
    lv_obj_set_style_radius(history_color_indicator, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(history_color_indicator, 0, 0);

    // --- Title Label (inside the new container) ---
    history_title_label = lv_label_create(title_container);
    lv_obj_set_flex_grow(history_title_label, 1); // Allow label to take remaining space
    lv_label_set_long_mode(history_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(history_title_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(history_title_label, &lv_font_montserrat_20, 0);
    
    // --- Main content container for calendar ---
    history_content_container = lv_obj_create(panel_show_history);
    lv_obj_remove_style_all(history_content_container);
    lv_obj_set_size(history_content_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(history_content_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(history_content_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(history_content_container, 8, 0);

    // --- Left side: Day of week labels ---
    lv_obj_t* day_labels_cont = lv_obj_create(history_content_container);
    lv_obj_remove_style_all(day_labels_cont);
    const int GRID_HEIGHT = 164; // 7*20 (cells) + 6*4 (gaps)
    lv_obj_set_height(day_labels_cont, GRID_HEIGHT);
    lv_obj_set_width(day_labels_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(day_labels_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(day_labels_cont, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    const char* day_labels[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    for (int i = 0; i < 7; i++) {
        lv_obj_t* label = lv_label_create(day_labels_cont);
        lv_label_set_text(label, day_labels[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_GREY), 0);
    }
    
    // --- Right side: Grid of squares ---
    const int NUM_WEEKS = 7;
    const int NUM_DAYS = 7;
    const int CELL_SIZE = 20;
    const int GAP_SIZE = 4;

    static lv_coord_t col_dsc[NUM_WEEKS + 1];
    static lv_coord_t row_dsc[NUM_DAYS + 1];
    for(int i = 0; i < NUM_WEEKS; i++) col_dsc[i] = CELL_SIZE;
    col_dsc[NUM_WEEKS] = LV_GRID_TEMPLATE_LAST;
    for(int i = 0; i < NUM_DAYS; i++) row_dsc[i] = CELL_SIZE;
    row_dsc[NUM_DAYS] = LV_GRID_TEMPLATE_LAST;

    lv_obj_t* grid = lv_obj_create(history_content_container);
    lv_obj_remove_style_all(grid);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(grid, GAP_SIZE, 0);
    lv_obj_set_style_pad_row(grid, GAP_SIZE, 0);
    lv_obj_set_size(grid, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    for (int week = 0; week < NUM_WEEKS; week++) {
        for (int day = 0; day < NUM_DAYS; day++) {
            lv_obj_t* cell = lv_obj_create(grid);
            lv_obj_remove_style_all(cell);
            lv_obj_add_style(cell, &style_calendar_cell, 0);
            lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, week, 1, LV_GRID_ALIGN_STRETCH, day, 1);
        }
    }

    // --- Streak Label at the bottom ---
    streak_label = lv_label_create(panel_show_history);
    lv_label_set_text(streak_label, "Current Streak: 0 days");
    lv_obj_set_style_text_font(streak_label, &lv_font_montserrat_16, 0);
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
        update_history_display();
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

int HabitHistoryView::calculate_streak(const std::vector<time_t>& completed_dates) const {
    if (completed_dates.empty()) {
        return 0;
    }

    time_t now = time(NULL);

    // Lambda to check if a specific date exists in the sorted vector
    auto is_date_completed = [&](time_t date_to_find) {
        // Use a custom comparator for binary_search to match by day, not by exact second
        return std::binary_search(completed_dates.begin(), completed_dates.end(), date_to_find, 
            [](time_t a, time_t b){ 
                // This complex lambda helps find if any timestamp 'a' from the vector
                // falls on the same day as a target timestamp 'b', respecting sort order.
                // It returns true if day of 'a' is less than day of 'b'.
                struct tm tm_a, tm_b;
                localtime_r(&a, &tm_a);
                localtime_r(&b, &tm_b);
                if (tm_a.tm_year != tm_b.tm_year) return tm_a.tm_year < tm_b.tm_year;
                if (tm_a.tm_mon != tm_b.tm_mon) return tm_a.tm_mon < tm_b.tm_mon;
                return tm_a.tm_mday < tm_b.tm_mday;
            });
    };
    
    // Check if the habit was completed today. If not, the current streak must be 0.
    if (!is_date_completed(now)) {
        return 0;
    }

    int streak = 1;
    time_t check_date = now - 86400; // Start checking from yesterday

    // Loop backwards in time
    while (true) {
        if (is_date_completed(check_date)) {
            streak++;
            check_date -= 86400; // Go back one more day
        } else {
            // The day was missed, the streak is broken
            break; 
        }
    }

    return streak;
}


void HabitHistoryView::update_history_display() {
    Habit* habit = HabitDataManager::get_habit_by_id(this->selected_habit_id);
    if (!habit) {
        ESP_LOGE(TAG, "Cannot show history, habit with ID %lu not found!", this->selected_habit_id);
        switch_to_step(HabitHistoryStep::SELECT_HABIT);
        return;
    }
    
    // Update the title and color indicator
    this->selected_habit_name = habit->name;
    lv_color_t habit_color = lv_color_hex(std::stoul(habit->color_hex.substr(1), nullptr, 16));

    lv_label_set_text(history_title_label, selected_habit_name.c_str());
    lv_obj_set_style_text_color(history_title_label, lv_color_black(), 0); // Title is now always black
    lv_obj_set_style_bg_color(history_color_indicator, habit_color, 0); // Set circle color
    
    // --- SOLUCIÓN: Hacer que el círculo sea opaco ---
    lv_obj_set_style_bg_opa(history_color_indicator, LV_OPA_COVER, 0);

    HabitHistory history = HabitDataManager::get_history_for_habit(this->selected_habit_id);
    // Ensure dates are sorted for efficient searching
    std::sort(history.completed_dates.begin(), history.completed_dates.end());
    
    // --- Date calculations for the grid ---
    time_t now = time(NULL);
    struct tm timeinfo_today;
    localtime_r(&now, &timeinfo_today);
    // Convert tm_wday (Sun=0, Mon=1...) to our grid's row index (Mon=0, ..., Sun=6)
    int today_grid_row = (timeinfo_today.tm_wday == 0) ? 6 : timeinfo_today.tm_wday - 1;

    // --- Update the calendar grid ---
    const int NUM_WEEKS = 7;
    const int NUM_DAYS = 7;
    lv_obj_t* grid = lv_obj_get_child(history_content_container, 1);

    for (int week = 0; week < NUM_WEEKS; week++) {
        for (int day = 0; day < NUM_DAYS; day++) {
            lv_obj_t* cell = lv_obj_get_child(grid, week * NUM_DAYS + day);

            // Calculate the date for the current cell
            int days_ago = ((NUM_WEEKS - 1 - week) * 7) + (today_grid_row - day);
            time_t cell_date = now - (time_t)days_ago * 86400; // 86400 seconds in a day

            // Reset cell style to default gray
            lv_obj_remove_style(cell, &style_calendar_cell_today, 0);
            lv_obj_set_style_bg_color(cell, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);

            // Check if habit was completed on this day using a binary search for efficiency
            if (std::binary_search(history.completed_dates.begin(), history.completed_dates.end(), cell_date, 
                // Custom comparator to check by day, not exact time
                [&](time_t a, time_t b){ return !is_same_day(a, b) && a < b; })) {
                 // Set the cell to a standard green color on completion
                 lv_obj_set_style_bg_color(cell, lv_palette_main(LV_PALETTE_LIGHT_GREEN), 0);
            }

            // Check if it's today and apply red border on top
            if (days_ago == 0) {
                lv_obj_add_style(cell, &style_calendar_cell_today, 0);
            }
        }
    }
    
    // --- Calculate and display streak ---
    int streak_count = calculate_streak(history.completed_dates);
    lv_label_set_text_fmt(streak_label, "Current Streak: %d days", streak_count);
    
    ESP_LOGI(TAG, "History display updated for habit '%s'.", habit->name.c_str());
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