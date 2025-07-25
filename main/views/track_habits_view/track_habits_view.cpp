#include "track_habits_view.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/habit_data_manager/habit_data_manager.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "TRACK_HABITS_VIEW";

// --- Lifecycle Methods ---

TrackHabitsView::TrackHabitsView() {
    ESP_LOGI(TAG, "Constructed");
}

TrackHabitsView::~TrackHabitsView() {
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

void TrackHabitsView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);

    init_styles();
    setup_ui(container);
    populate_list();
    setup_button_handlers();
}

// --- UI Setup ---

void TrackHabitsView::init_styles() {
    if (styles_initialized) return;
    lv_style_init(&style_focused_list_btn);
    lv_style_set_bg_color(&style_focused_list_btn, lv_palette_lighten(LV_PALETTE_BLUE, 2));
    lv_style_set_border_color(&style_focused_list_btn, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_width(&style_focused_list_btn, 2);
    styles_initialized = true;
}

void TrackHabitsView::reset_styles() {
    if (!styles_initialized) return;
    lv_style_reset(&style_focused_list_btn);
    styles_initialized = false;
}

void TrackHabitsView::setup_ui(lv_obj_t* parent) {
    group = lv_group_create();
    lv_group_set_wrap(group, true);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(parent, 10, 0);
    lv_obj_set_style_pad_top(parent, 15, 0);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Today's Habits");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    list_habits = lv_list_create(parent);
    lv_obj_set_size(list_habits, LV_PCT(95), LV_PCT(80));
    lv_obj_center(list_habits);
    lv_group_set_default(group);
}

void TrackHabitsView::populate_list() {
    lv_obj_clean(list_habits);
    lv_group_remove_all_objs(group);
    displayed_habits.clear();

    // Collect all active habits from all active categories
    auto categories = HabitDataManager::get_active_categories();
    for (const auto& category : categories) {
        auto habits_in_cat = HabitDataManager::get_active_habits_for_category(category.id);
        displayed_habits.insert(displayed_habits.end(), habits_in_cat.begin(), habits_in_cat.end());
    }

    if (displayed_habits.empty()) {
        lv_list_add_text(list_habits, "No active habits found.\nGo to 'Manage Habits' to add one.");
        return;
    }
    
    for (const auto& habit : displayed_habits) {
        lv_obj_t* btn = create_habit_list_item(list_habits, habit);
        lv_obj_add_style(btn, &style_focused_list_btn, LV_STATE_FOCUSED);
        lv_obj_set_user_data(btn, (void*)habit.id); // Store habit ID for easy access
        lv_group_add_obj(group, btn);
    }
}

lv_obj_t* TrackHabitsView::create_habit_list_item(lv_obj_t* parent, const Habit& habit) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, LV_PCT(100), 50);
    
    // --- MODIFICATION ---
    // The following line caused a warning and is not essential for a "flat" button look, so it's removed for simplicity.
    // lv_obj_remove_style(btn, NULL, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(btn, 10, 0);

    // Left container for color circle and name
    lv_obj_t* left_cont = lv_obj_create(btn);
    lv_obj_remove_style_all(left_cont);
    lv_obj_set_size(left_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(left_cont, 10, 0);

    // Color Circle
    lv_obj_t* color_circle = lv_obj_create(left_cont);
    lv_obj_set_size(color_circle, 15, 15);
    lv_obj_set_style_radius(color_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(color_circle, lv_color_hex(std::stoul(habit.color_hex.substr(1), nullptr, 16)), 0);
    lv_obj_set_style_border_width(color_circle, 0, 0);
    
    // Habit Name
    lv_obj_t* label = lv_label_create(left_cont);
    lv_label_set_text(label, habit.name.c_str());
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label, 120); // Limit width to allow for scrolling

    // Checkbox
    lv_obj_t* cb = lv_checkbox_create(btn);

    // --- MODIFIED ---
    // Cast the result of the bitwise OR to the correct enum type to fix the C++ type error.
    lv_obj_add_flag(cb, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_CLICKABLE));
    
    // Set initial state
    if (HabitDataManager::is_habit_done_today(habit.id)) {
        lv_obj_add_state(cb, LV_STATE_CHECKED);
    }
    
    return btn;
}

// --- Button Handling ---

void TrackHabitsView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press_cb, true, this);
}

void TrackHabitsView::on_ok_press() {
    if (displayed_habits.empty()) return;

    lv_obj_t* focused_btn = lv_group_get_focused(group);
    if (!focused_btn) return;

    uint32_t habit_id = (uint32_t)lv_obj_get_user_data(focused_btn);
    // The checkbox is the second child of the button (index 1)
    lv_obj_t* cb = lv_obj_get_child(focused_btn, 1);
    if (!cb) return;

    time_t now = time(NULL);
    bool is_checked = lv_obj_has_state(cb, LV_STATE_CHECKED);

    if (is_checked) {
        // It was done, now un-mark it
        if (HabitDataManager::unmark_habit_as_done(habit_id, now)) {
            lv_obj_clear_state(cb, LV_STATE_CHECKED);
            ESP_LOGI(TAG, "Unmarked habit %lu as done for today.", habit_id);
        } else {
            ESP_LOGE(TAG, "Failed to unmark habit %lu.", habit_id);
        }
    } else {
        // It was not done, now mark it
        if (HabitDataManager::mark_habit_as_done(habit_id, now)) {
            lv_obj_add_state(cb, LV_STATE_CHECKED);
            ESP_LOGI(TAG, "Marked habit %lu as done for today.", habit_id);
        } else {
            ESP_LOGE(TAG, "Failed to mark habit %lu.", habit_id);
        }
    }
}

void TrackHabitsView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed, returning to habit menu.");
    view_manager_load_view(VIEW_ID_HABIT_MANAGER);
}

void TrackHabitsView::on_nav_press(bool next) {
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

// --- Static Callbacks ---
void TrackHabitsView::handle_ok_press_cb(void* user_data) { static_cast<TrackHabitsView*>(user_data)->on_ok_press(); }
void TrackHabitsView::handle_cancel_press_cb(void* user_data) { static_cast<TrackHabitsView*>(user_data)->on_cancel_press(); }
void TrackHabitsView::handle_left_press_cb(void* user_data) { static_cast<TrackHabitsView*>(user_data)->on_nav_press(false); }
void TrackHabitsView::handle_right_press_cb(void* user_data) { static_cast<TrackHabitsView*>(user_data)->on_nav_press(true); }