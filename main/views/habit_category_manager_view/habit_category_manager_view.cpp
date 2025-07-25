#include "habit_category_manager_view.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/habit_data_manager/habit_data_manager.h" // <-- The view now uses the data manager
#include "esp_log.h"
#include <time.h>
#include <vector>
#include <cstring>

static const char *TAG = "HABIT_CAT_MGR_VIEW";

HabitCategoryManagerView::HabitCategoryManagerView() {
    ESP_LOGI(TAG, "Constructed");
}

HabitCategoryManagerView::~HabitCategoryManagerView() {
    ESP_LOGI(TAG, "Destructed");
    destroy_action_menu();
    reset_styles();
    if (main_group) {
        if (lv_group_get_default() == main_group) {
            lv_group_set_default(nullptr);
        }
        lv_group_delete(main_group);
        main_group = nullptr;
    }
}

void HabitCategoryManagerView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);

    setup_ui(container);
    setup_button_handlers();
    repopulate_category_slots();
}

void HabitCategoryManagerView::setup_ui(lv_obj_t* parent) {
    init_styles();
    main_group = lv_group_create();
    lv_group_set_wrap(main_group, true);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(parent, 10, 0);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Manage Categories");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    category_container = lv_obj_create(parent);
    lv_obj_remove_style_all(category_container);
    lv_obj_set_size(category_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(category_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(category_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(category_container, 10, 0);
}

void HabitCategoryManagerView::repopulate_category_slots() {
    lv_obj_clean(category_container);
    lv_group_remove_all_objs(main_group);

    // Get the current list of active categories from the data manager
    auto active_categories = HabitDataManager::get_active_categories();
    const int MAX_UI_CATEGORIES = 3; // The number of user-configurable slots in the UI
    const int total_slots = 4;

    for (int i = 0; i < total_slots; ++i) {
        lv_obj_t* slot = lv_button_create(category_container);
        lv_obj_set_size(slot, LV_PCT(90), 45);
        lv_obj_add_style(slot, &style_focused, LV_STATE_FOCUSED);
        lv_group_add_obj(main_group, slot);
        
        lv_obj_t* label = lv_label_create(slot);
        lv_obj_center(label);

        if (i < MAX_UI_CATEGORIES) {
            if (i < active_categories.size()) {
                const auto& category = active_categories[i];
                int habit_count = HabitDataManager::get_habit_count_for_category(category.id, true);
                lv_label_set_text_fmt(label, "%s (%d)", category.name.c_str(), habit_count);
            } else {
                lv_label_set_text(label, LV_SYMBOL_PLUS " Add Category");
            }
        } else {
            lv_label_set_text(label, "General");
            // Here you could get the count for a general/uncategorized category if you have one
            // For now, it's just a label.
        }
    }
    
    if(lv_group_get_obj_count(main_group) > 0) {
        lv_group_focus_obj(lv_group_get_obj_by_index(main_group, 0));
    }
    lv_group_set_default(main_group);
}

void HabitCategoryManagerView::setup_button_handlers() {
    set_main_input_active(true);
}

void HabitCategoryManagerView::on_ok_press() {
    lv_obj_t* focused_btn = lv_group_get_focused(main_group);
    if (!focused_btn) return;

    uint32_t slot_index = lv_obj_get_index(focused_btn);
    auto active_categories = HabitDataManager::get_active_categories();
    const int MAX_UI_CATEGORIES = 3;
    
    if (slot_index < MAX_UI_CATEGORIES) {
        if (slot_index < active_categories.size()) {
            // It's an existing category, show action menu
            uint32_t category_id = active_categories[slot_index].id;
            ESP_LOGI(TAG, "Selected existing category with ID %lu", category_id);
            create_action_menu(category_id);
        } else {
            // It's an "+ Add Category" button
            ESP_LOGI(TAG, "Adding new category at slot %lu...", slot_index);
            time_t now = time(NULL);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            char name_buffer[64];
            strftime(name_buffer, sizeof(name_buffer), "Cat_%y%m%d_%H%M%S", &timeinfo);
            
            HabitDataManager::add_category(std::string(name_buffer));
            repopulate_category_slots();
        }
    } else {
        // The "General" category was selected. Could navigate to its habit list.
        ESP_LOGI(TAG, "Selected 'General' category. (No action implemented)");
    }
}

void HabitCategoryManagerView::on_cancel_press() {
    view_manager_load_view(VIEW_ID_HABIT_MANAGER);
}

void HabitCategoryManagerView::on_nav_press(bool next) {
    if (next) lv_group_focus_next(main_group);
    else lv_group_focus_prev(main_group);
}

// --- Action Menu Logic ---
void HabitCategoryManagerView::create_action_menu(uint32_t category_id) {
    if (action_menu_container) return;
    this->selected_category_id = category_id;
    set_main_input_active(false);

    action_menu_container = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(action_menu_container);
    lv_obj_set_size(action_menu_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(action_menu_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(action_menu_container, LV_OPA_50, 0);

    lv_obj_t* list = lv_list_create(action_menu_container);
    lv_obj_set_width(list, 180);
    lv_obj_center(list);

    action_menu_group = lv_group_create();
    lv_group_set_wrap(action_menu_group, true);

    lv_obj_t* btn;
    btn = lv_list_add_button(list, LV_SYMBOL_EYE_OPEN, "View Details");
    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(action_menu_group, btn);

    btn = lv_list_add_button(list, LV_SYMBOL_TRASH, "Archive"); // Changed from Delete to Archive
    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(action_menu_group, btn);

    lv_group_set_default(action_menu_group);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_action_menu_ok_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_action_menu_cancel_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_action_menu_nav_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_action_menu_nav_cb, true, this);
}

void HabitCategoryManagerView::destroy_action_menu() {
    if (!action_menu_container) return;
    lv_obj_del(action_menu_container);
    action_menu_container = nullptr;
    if (action_menu_group) {
        lv_group_del(action_menu_group);
        action_menu_group = nullptr;
    }
    this->selected_category_id = 0;
    set_main_input_active(true);
}

void HabitCategoryManagerView::on_action_menu_ok() {
    if (!action_menu_group || this->selected_category_id == 0) return;
    lv_obj_t* focused_btn = lv_group_get_focused(action_menu_group);
    if (!focused_btn) return;
    
    const char* action_text = lv_list_get_button_text(lv_obj_get_parent(focused_btn), focused_btn);

    if (strcmp(action_text, "Archive") == 0) {
        ESP_LOGI(TAG, "Archiving category with ID %lu", this->selected_category_id);
        HabitDataManager::archive_category(this->selected_category_id);
        destroy_action_menu();
        repopulate_category_slots();
    } else if (strcmp(action_text, "View Details") == 0) {
        ESP_LOGI(TAG, "View Details for category ID %lu selected (Not Implemented)", this->selected_category_id);
        destroy_action_menu();
    }
}

void HabitCategoryManagerView::on_action_menu_cancel() {
    destroy_action_menu();
}

void HabitCategoryManagerView::on_action_menu_nav(bool next) {
    if (!action_menu_group) return;
    if (next) lv_group_focus_next(action_menu_group);
    else lv_group_focus_prev(action_menu_group);
}

void HabitCategoryManagerView::set_main_input_active(bool active) {
    button_manager_unregister_view_handlers();
    if (active) {
        lv_group_set_default(main_group);
        button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press_cb, true, this);
        button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_cb, true, this);
        button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press_cb, true, this);
        button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press_cb, true, this);
    } else {
        if (lv_group_get_default() == main_group) {
            lv_group_set_default(nullptr);
        }
    }
}

// --- Style Management ---
void HabitCategoryManagerView::init_styles() {
    if (styles_initialized) return;
    lv_style_init(&style_focused);
    lv_style_set_bg_color(&style_focused, lv_palette_lighten(LV_PALETTE_BLUE, 2));
    lv_style_set_border_color(&style_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_width(&style_focused, 2);
    styles_initialized = true;
}

void HabitCategoryManagerView::reset_styles() {
    if (!styles_initialized) return;
    lv_style_reset(&style_focused);
    styles_initialized = false;
}

// --- Static Callbacks to bridge to instance methods ---
void HabitCategoryManagerView::handle_ok_press_cb(void* user_data) { static_cast<HabitCategoryManagerView*>(user_data)->on_ok_press(); }
void HabitCategoryManagerView::handle_cancel_press_cb(void* user_data) { static_cast<HabitCategoryManagerView*>(user_data)->on_cancel_press(); }
void HabitCategoryManagerView::handle_left_press_cb(void* user_data) { static_cast<HabitCategoryManagerView*>(user_data)->on_nav_press(false); }
void HabitCategoryManagerView::handle_right_press_cb(void* user_data) { static_cast<HabitCategoryManagerView*>(user_data)->on_nav_press(true); }

void HabitCategoryManagerView::handle_action_menu_ok_cb(void* user_data) { static_cast<HabitCategoryManagerView*>(user_data)->on_action_menu_ok(); }
void HabitCategoryManagerView::handle_action_menu_cancel_cb(void* user_data) { static_cast<HabitCategoryManagerView*>(user_data)->on_action_menu_cancel(); }
void HabitCategoryManagerView::handle_action_menu_nav_cb(void* user_data) { 
    static_cast<HabitCategoryManagerView*>(user_data)->on_action_menu_nav(true); 
}