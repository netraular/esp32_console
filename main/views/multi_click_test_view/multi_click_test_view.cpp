#include "multi_click_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"

static const char *TAG = "MULTI_CLICK_TEST_VIEW";

// --- UI Widgets ---
static lv_obj_t *event_labels[BUTTON_COUNT];
static const char* button_names[BUTTON_COUNT] = { "Left", "Cancel", "OK", "Right", "On/Off" };

// --- Private Function Prototypes ---
static void handle_event(button_id_t button, const char* event_name);
static void handle_cancel_press(void* user_data);

// --- Generic Event Handler ---
static void handle_event(button_id_t button, const char* event_name) {
    if (button < BUTTON_COUNT && event_labels[button]) {
        ESP_LOGI(TAG, "Button '%s' Event: %s", button_names[button], event_name);
        lv_label_set_text(event_labels[button], event_name);
    }
}

// --- Handler Implementations ---

// Button LEFT
static void h_left_down(void* user_data) { handle_event(BUTTON_LEFT, "Press Down"); }
static void h_left_up(void* user_data) { handle_event(BUTTON_LEFT, "Press Up"); }
static void h_left_tap(void* user_data) { handle_event(BUTTON_LEFT, "Tap (Fast)"); } // Tap handler
static void h_left_single(void* user_data) { handle_event(BUTTON_LEFT, "Single Click"); }
static void h_left_double(void* user_data) { handle_event(BUTTON_LEFT, "Double Click"); }
static void h_left_long_start(void* user_data) { handle_event(BUTTON_LEFT, "Long Press Start"); }
static void h_left_long_hold(void* user_data) { handle_event(BUTTON_LEFT, "Long Press Hold"); }

// Button CANCEL (used to exit)
static void handle_cancel_press(void* user_data) {
    ESP_LOGI(TAG, "Exiting view.");
    view_manager_load_view(VIEW_ID_MENU);
}

// Button OK
static void h_ok_down(void* user_data) { handle_event(BUTTON_OK, "Press Down"); }
static void h_ok_up(void* user_data) { handle_event(BUTTON_OK, "Press Up"); }
static void h_ok_tap(void* user_data) { handle_event(BUTTON_OK, "Tap (Fast)"); } // Tap handler
static void h_ok_single(void* user_data) { handle_event(BUTTON_OK, "Single Click"); }
static void h_ok_double(void* user_data) { handle_event(BUTTON_OK, "Double Click"); }
static void h_ok_long_start(void* user_data) { handle_event(BUTTON_OK, "Long Press Start"); }
static void h_ok_long_hold(void* user_data) { handle_event(BUTTON_OK, "Long Press Hold"); }

// Button RIGHT
static void h_right_down(void* user_data) { handle_event(BUTTON_RIGHT, "Press Down"); }
static void h_right_up(void* user_data) { handle_event(BUTTON_RIGHT, "Press Up"); }
static void h_right_tap(void* user_data) { handle_event(BUTTON_RIGHT, "Tap (Fast)"); } // Tap handler
static void h_right_single(void* user_data) { handle_event(BUTTON_RIGHT, "Single Click"); }
static void h_right_double(void* user_data) { handle_event(BUTTON_RIGHT, "Double Click"); }
static void h_right_long_start(void* user_data) { handle_event(BUTTON_RIGHT, "Long Press Start"); }
static void h_right_long_hold(void* user_data) { handle_event(BUTTON_RIGHT, "Long Press Hold"); }

// Button ON/OFF
static void h_onoff_down(void* user_data) { handle_event(BUTTON_ON_OFF, "Press Down"); }
static void h_onoff_up(void* user_data) { handle_event(BUTTON_ON_OFF, "Press Up"); }
static void h_onoff_tap(void* user_data) { handle_event(BUTTON_ON_OFF, "Tap (Fast)"); } // Tap handler
static void h_onoff_single(void* user_data) { handle_event(BUTTON_ON_OFF, "Single Click"); }
static void h_onoff_double(void* user_data) { handle_event(BUTTON_ON_OFF, "Double Click"); }
static void h_onoff_long_start(void* user_data) { handle_event(BUTTON_ON_OFF, "Long Press Start"); }
static void h_onoff_long_hold(void* user_data) { handle_event(BUTTON_ON_OFF, "Long Press Hold"); }


// --- Main View Creation Function ---
void multi_click_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Multi-Click Test View");
    button_manager_set_dispatch_mode(INPUT_DISPATCH_MODE_QUEUED);

    // --- Create UI ---
    lv_obj_t *main_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(main_cont, 10, 0);
    lv_obj_set_style_pad_gap(main_cont, 8, 0);

    lv_obj_t* title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "Button Event Test");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_bottom(title_label, 10, 0);

    static lv_coord_t col_dsc[] = {80, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(main_cont);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_size(grid, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_all(grid, 5, 0);
    lv_obj_set_style_pad_gap(grid, 5, 0);

    for (int i = 0; i < BUTTON_COUNT; i++) {
        lv_obj_t *name_lbl = lv_label_create(grid);
        lv_obj_set_grid_cell(name_lbl, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, i, 1);
        lv_label_set_text(name_lbl, button_names[i]);

        event_labels[i] = lv_label_create(grid);
        lv_obj_set_grid_cell(event_labels[i], LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, i, 1);
        lv_label_set_text(event_labels[i], "---");
    }

    lv_obj_t* instructions_label = lv_label_create(main_cont);
    lv_label_set_text(instructions_label, "Press CANCEL to exit");
    lv_obj_set_style_margin_top(instructions_label, 15, 0);

    // --- Register Button Handlers ---
    
    // LEFT
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_PRESS_DOWN, h_left_down, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_PRESS_UP, h_left_up, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, h_left_tap, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_SINGLE_CLICK, h_left_single, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_DOUBLE_CLICK, h_left_double, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_LONG_PRESS_START, h_left_long_start, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_LONG_PRESS_HOLD, h_left_long_hold, true, nullptr);
    
    // CANCEL
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);

    // OK
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_PRESS_DOWN, h_ok_down, true, nullptr);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_PRESS_UP, h_ok_up, true, nullptr);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, h_ok_tap, true, nullptr);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_SINGLE_CLICK, h_ok_single, true, nullptr);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_DOUBLE_CLICK, h_ok_double, true, nullptr);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_LONG_PRESS_START, h_ok_long_start, true, nullptr);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_LONG_PRESS_HOLD, h_ok_long_hold, true, nullptr);

    // RIGHT
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_PRESS_DOWN, h_right_down, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_PRESS_UP, h_right_up, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, h_right_tap, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_SINGLE_CLICK, h_right_single, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_DOUBLE_CLICK, h_right_double, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, h_right_long_start, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_HOLD, h_right_long_hold, true, nullptr);

    // ON/OFF
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_PRESS_DOWN, h_onoff_down, true, nullptr);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_PRESS_UP, h_onoff_up, true, nullptr);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_TAP, h_onoff_tap, true, nullptr);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_SINGLE_CLICK, h_onoff_single, true, nullptr);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_DOUBLE_CLICK, h_onoff_double, true, nullptr);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_LONG_PRESS_START, h_onoff_long_start, true, nullptr);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_LONG_PRESS_HOLD, h_onoff_long_hold, true, nullptr);
}