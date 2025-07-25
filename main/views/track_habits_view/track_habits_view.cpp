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

    // Initial setup
    init_styles();
    setup_ui(container);
    setup_button_handlers();

    // Initial data population
    populate_habit_list();
}

// --- Style Management ---

void TrackHabitsView::init_styles() {
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
    ESP_LOGD(TAG, "Styles initialized.");
}

void TrackHabitsView::reset_styles() {
    if (!styles_initialized) return;
    lv_style_reset(&style_list_item_focused);
    lv_style_reset(&style_category_header);
    styles_initialized = false;
    ESP_LOGD(TAG, "Styles reset.");
}

// --- UI Setup ---

void TrackHabitsView::setup_ui(lv_obj_t* parent) {
    group = lv_group_create();
    lv_group_set_wrap(group, false); // Do not wrap around the list

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(parent, 5, 0);
    lv_obj_set_style_pad_hor(parent, 5, 0);
    lv_obj_set_style_pad_ver(parent, 10, 0);

    // Title
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Track Today's Habits");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);


    // Container for the list of habits
    habit_list_container = lv_obj_create(parent);
    lv_obj_remove_style_all(habit_list_container);
    lv_obj_set_width(habit_list_container, LV_PCT(100));
    lv_obj_set_flex_grow(habit_list_container, 1); // Make it take remaining space
    lv_obj_set_flex_flow(habit_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(habit_list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(habit_list_container, 5, 0);

    lv_group_set_default(group);
}

void TrackHabitsView::populate_habit_list() {
    ESP_LOGI(TAG, "Populating unified habit list...");
    
    // 1. Clear previous state
    lv_obj_clean(habit_list_container);
    m_habit_render_data.clear();
    lv_group_remove_all_objs(group);

    // 2. Fetch all active categories
    auto all_categories = HabitDataManager::get_active_categories();
    
    // --- FIX: First, count all habits to reserve vector capacity ---
    size_t total_habit_count = 0;
    for (const auto& category : all_categories) {
        total_habit_count += HabitDataManager::get_active_habits_for_category(category.id).size();
    }

    if (total_habit_count == 0) {
        lv_obj_t* label = lv_label_create(habit_list_container);
        lv_label_set_text(label, "No habits created yet.\nGo to 'Manage Habits' to add one.");
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        ESP_LOGW(TAG, "No active habits found in any category.");
        return;
    }
    
    // Reserve memory to prevent reallocations and dangling pointers
    ESP_LOGD(TAG, "Reserving space for %d habits.", total_habit_count);
    m_habit_render_data.reserve(total_habit_count);
    // --- END FIX ---

    // 3. Build the UI and data list
    for (const auto& category : all_categories) {
        auto habits_in_cat = HabitDataManager::get_active_habits_for_category(category.id);

        if (habits_in_cat.empty()) {
            continue; // Skip categories with no active habits
        }
        
        // Add a non-focusable header for the category
        lv_obj_t* header = lv_label_create(habit_list_container);
        lv_label_set_text(header, category.name.c_str());
        lv_obj_add_style(header, &style_category_header, 0);
        lv_obj_set_width(header, LV_PCT(95));
        lv_obj_set_style_pad_top(header, 10, 0);

        // Add habits for this category
        for (const auto& habit : habits_in_cat) {
            m_habit_render_data.push_back({
                .habit = habit,
                .category_name = category.name,
                .is_done_today = HabitDataManager::is_habit_done_today(habit.id)
            });

            // Get a pointer to the newly added element, which is now stable.
            HabitRenderData* data_ptr = &m_habit_render_data.back();

            // Create the list item container
            lv_obj_t* item = lv_obj_create(habit_list_container);
            lv_obj_remove_style_all(item);
            lv_obj_set_size(item, LV_PCT(95), 40);
            lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(item, 10, 0);
            lv_obj_set_style_radius(item, 5, 0);
            lv_obj_add_style(item, &style_list_item_focused, LV_STATE_FOCUSED);
            
            // Pass the STABLE pointer to the data in the vector
            lv_obj_set_user_data(item, data_ptr);
            
            // Color indicator, checkbox, and label
            lv_obj_t* color_indicator = lv_obj_create(item);
            lv_obj_set_size(color_indicator, 10, 25);
            lv_obj_set_style_radius(color_indicator, 3, 0);
            lv_obj_set_style_border_width(color_indicator, 0, 0);
            lv_obj_set_style_bg_color(color_indicator, lv_color_hex(std::stoul(data_ptr->habit.color_hex.substr(1), nullptr, 16)), 0);

            lv_obj_t* cb = lv_checkbox_create(item);
            lv_checkbox_set_text(cb, "");
            lv_obj_add_state(cb, LV_STATE_DISABLED);
            if (data_ptr->is_done_today) {
                lv_obj_add_state(cb, LV_STATE_CHECKED);
            }
            
            lv_obj_t* label = lv_label_create(item);
            lv_label_set_text(label, data_ptr->habit.name.c_str());
            lv_obj_set_flex_grow(label, 1);
            lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);

            lv_group_add_obj(group, item);
        }
    }
    ESP_LOGI(TAG, "Habit list populated with %d items.", m_habit_render_data.size());
}


// --- Button and Event Handling ---

void TrackHabitsView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press_cb, true, this); // Up
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press_cb, true, this); // Down
}

void TrackHabitsView::on_ok_press() {
    ESP_LOGI(TAG, "OK Press");
    lv_obj_t* focused_obj = lv_group_get_focused(group);
    if (!focused_obj) {
        ESP_LOGE(TAG, "OK pressed but no object is focused.");
        return;
    }

    auto* data = static_cast<HabitRenderData*>(lv_obj_get_user_data(focused_obj));
    if (!data) {
        ESP_LOGE(TAG, "OK pressed on list item, but user data is invalid!");
        return;
    }

    // This log should now be safe
    ESP_LOGI(TAG, "Toggling habit: '%s' (ID: %lu)", data->habit.name.c_str(), data->habit.id);

    // Toggle the state in our local cache and the database
    data->is_done_today = !data->is_done_today;
    time_t now = time(NULL);

    if (data->is_done_today) {
        ESP_LOGI(TAG, "Marking as DONE");
        HabitDataManager::mark_habit_as_done(data->habit.id, now);
    } else {
        ESP_LOGI(TAG, "Marking as NOT DONE");
        HabitDataManager::unmark_habit_as_done(data->habit.id, now);
    }

    // Update the checkbox UI efficiently
    lv_obj_t* cb = lv_obj_get_child(focused_obj, 1); // Child 0: color, Child 1: checkbox
    if (cb) {
        if (data->is_done_today) {
            lv_obj_add_state(cb, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(cb, LV_STATE_CHECKED);
        }
        ESP_LOGD(TAG, "Checkbox UI updated.");
    } else {
        ESP_LOGE(TAG, "Could not find checkbox object to update!");
    }
}

void TrackHabitsView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel Press: Returning to Habit Manager.");
    view_manager_load_view(VIEW_ID_HABIT_MANAGER);
}

void TrackHabitsView::on_nav_press(bool next) {
    if (lv_group_get_obj_count(group) == 0) return;

    if (next) {
        ESP_LOGD(TAG, "Navigating DOWN (Next)");
        lv_group_focus_next(group);
    } else {
        ESP_LOGD(TAG, "Navigating UP (Previous)");
        lv_group_focus_prev(group);
    }
    // Ensure the focused item is visible
    lv_obj_t* focused = lv_group_get_focused(group);
    if (focused) {
        lv_obj_scroll_to_view_recursive(focused, LV_ANIM_ON);
    }
}

// --- Static Callbacks ---
void TrackHabitsView::handle_ok_press_cb(void* user_data) {
    static_cast<TrackHabitsView*>(user_data)->on_ok_press();
}

void TrackHabitsView::handle_cancel_press_cb(void* user_data) {
    static_cast<TrackHabitsView*>(user_data)->on_cancel_press();
}

void TrackHabitsView::handle_left_press_cb(void* user_data) {
    // Left button acts as "Up"
    static_cast<TrackHabitsView*>(user_data)->on_nav_press(false);
}

void TrackHabitsView::handle_right_press_cb(void* user_data) {
    // Right button acts as "Down"
    static_cast<TrackHabitsView*>(user_data)->on_nav_press(true);
}