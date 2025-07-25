#include "habit_category_manager_view.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/littlefs_manager/littlefs_manager.h"
#include "esp_log.h"
#include <time.h>
#include <vector>
#include <cstring>
#include <sstream>

static const char *TAG = "HABIT_CAT_MGR_VIEW";
static const char* CATEGORY_FILENAME = "categories.dat";

// --- Static Data Initialization ---
std::vector<HabitCategory> HabitCategoryManagerView::user_categories = {};
const int HabitCategoryManagerView::MAX_USER_CATEGORIES = 3;

HabitCategoryManagerView::HabitCategoryManagerView() {
    ESP_LOGI(TAG, "Constructed");
    if (user_categories.empty()) {
        load_categories_from_fs();
    }
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

    const int total_slots = 4;
    for (int i = 0; i < total_slots; ++i) {
        lv_obj_t* slot = lv_button_create(category_container);
        lv_obj_set_size(slot, LV_PCT(90), 45);
        lv_obj_add_style(slot, &style_focused, LV_STATE_FOCUSED);
        lv_group_add_obj(main_group, slot);
        
        lv_obj_t* label = lv_label_create(slot);
        lv_obj_center(label);

        if (i < MAX_USER_CATEGORIES) {
            if (i < user_categories.size()) {
                const auto& category = user_categories[i];
                lv_label_set_text_fmt(label, "%s (%d)", category.name.c_str(), category.habit_count);
            } else {
                lv_label_set_text(label, LV_SYMBOL_PLUS " Add Category");
            }
        } else {
            lv_label_set_text(label, "General");
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
    
    if (slot_index < MAX_USER_CATEGORIES) {
        if (slot_index < user_categories.size()) {
            ESP_LOGI(TAG, "Selected existing category at slot %lu", slot_index);
            create_action_menu(slot_index);
        } else {
            ESP_LOGI(TAG, "Adding new category at slot %lu...", slot_index);
            time_t now = time(NULL);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            char name_buffer[64];
            strftime(name_buffer, sizeof(name_buffer), "Cat_%y%m%d_%H%M%S", &timeinfo);
            
            user_categories.push_back({std::string(name_buffer), 0});
            save_categories_to_fs(); // Save after adding
            repopulate_category_slots();
        }
    } else {
        ESP_LOGI(TAG, "Selected 'General' category (slot %lu). No action.", slot_index);
    }
}

void HabitCategoryManagerView::on_cancel_press() {
    view_manager_load_view(VIEW_ID_HABIT_MANAGER);
}

void HabitCategoryManagerView::on_nav_press(bool next) {
    if (next) lv_group_focus_next(main_group);
    else lv_group_focus_prev(main_group);
}


// --- Filesystem Persistence ---
void HabitCategoryManagerView::load_categories_from_fs() {
    ESP_LOGI(TAG, "Attempting to load categories from '%s'", CATEGORY_FILENAME);
    user_categories.clear();

    char* buffer = nullptr;
    size_t size = 0;
    if (littlefs_manager_read_file(CATEGORY_FILENAME, &buffer, &size) && buffer) {
        std::stringstream ss(buffer);
        std::string line;
        
        while (std::getline(ss, line)) {
            std::stringstream line_ss(line);
            std::string name;
            std::string count_str;
            
            if (std::getline(line_ss, name, ',') && std::getline(line_ss, count_str)) {
                // --- MODIFICATION: Replaced try-catch with exception-free conversion ---
                std::stringstream count_converter(count_str);
                int count = 0;
                // Attempt to extract the integer. This expression is true if successful.
                if (count_converter >> count) {
                    user_categories.push_back({name, count});
                } else {
                    ESP_LOGE(TAG, "Invalid number format in categories file: '%s'. Using 0.", count_str.c_str());
                    user_categories.push_back({name, 0}); // Add with a default value
                }
                // --- End of Modification ---
            }
        }
        free(buffer);
        ESP_LOGI(TAG, "Loaded %d categories.", user_categories.size());
    } else {
        ESP_LOGI(TAG, "Categories file not found or empty. Starting fresh.");
    }
}

void HabitCategoryManagerView::save_categories_to_fs() {
    ESP_LOGI(TAG, "Saving %d categories to '%s'", user_categories.size(), CATEGORY_FILENAME);
    std::stringstream ss;
    for (const auto& category : user_categories) {
        ss << category.name << "," << category.habit_count << "\n";
    }

    if (!littlefs_manager_write_file(CATEGORY_FILENAME, ss.str().c_str())) {
        ESP_LOGE(TAG, "Failed to save categories to filesystem!");
    }
}


// --- Action Menu Logic ---
void HabitCategoryManagerView::create_action_menu(int category_index) {
    if (action_menu_container) return;
    this->selected_category_index = category_index;
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

    btn = lv_list_add_button(list, LV_SYMBOL_TRASH, "Delete");
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
    this->selected_category_index = -1;
    set_main_input_active(true);
}

void HabitCategoryManagerView::on_action_menu_ok() {
    if (!action_menu_group) return;
    lv_obj_t* focused_btn = lv_group_get_focused(action_menu_group);
    if (!focused_btn) return;
    
    const char* action_text = lv_list_get_button_text(lv_obj_get_parent(focused_btn), focused_btn);

    if (strcmp(action_text, "Delete") == 0) {
        if (selected_category_index >= 0 && selected_category_index < user_categories.size()) {
            ESP_LOGI(TAG, "Deleting category: %s", user_categories[selected_category_index].name.c_str());
            user_categories.erase(user_categories.begin() + selected_category_index);
            save_categories_to_fs(); // Save after deleting
            destroy_action_menu();
            repopulate_category_slots();
        }
    } else if (strcmp(action_text, "View Details") == 0) {
        ESP_LOGI(TAG, "View Details for '%s' selected (Not Implemented)", user_categories[selected_category_index].name.c_str());
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