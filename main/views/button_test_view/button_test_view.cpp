#include "button_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"

static const char *TAG = "BUTTON_TEST_VIEW";

// --- UI Widgets ---
static lv_obj_t *mode_label;
static lv_obj_t *last_press_label;
static lv_obj_t *counter_label;

// --- State Variables ---
static input_dispatch_mode_t current_mode;
static volatile uint32_t immediate_press_counter;
static lv_timer_t *ui_update_timer;

// --- Private Function Prototypes ---
static void update_ui_labels();
static void handle_ok_press();
static void handle_cancel_press();
static void handle_test_button_press(const char* btn_name);
static void handle_test_button_left() { handle_test_button_press("LEFT"); }
static void handle_test_button_right() { handle_test_button_press("RIGHT"); }
static void handle_test_button_on_off() { handle_test_button_press("ON/OFF"); }
static void ui_update_timer_cb(lv_timer_t *timer);

// --- Implementation ---

// Timer to safely update the UI from the LVGL thread
static void ui_update_timer_cb(lv_timer_t *timer) {
    if (current_mode == INPUT_DISPATCH_MODE_IMMEDIATE) {
        // CORRECCIÓN: Usar %lu para uint32_t
        lv_label_set_text_fmt(counter_label, "Immediate Count: %lu", immediate_press_counter);
    }
}

// Handler for the test buttons (Left, Right, On/Off)
static void handle_test_button_press(const char* btn_name) {
    if (current_mode == INPUT_DISPATCH_MODE_QUEUED) {
        // SAFE: We are in QUEUED mode, so this handler is called from the LVGL timer context.
        // We can update the UI directly.
        ESP_LOGI(TAG, "Button '%s' press handled in QUEUED mode.", btn_name);
        lv_label_set_text_fmt(last_press_label, "Last Queued Press: %s", btn_name);
    } else {
        // UNSAFE to call LVGL here. This handler is called directly from the button task/ISR.
        // We update a volatile variable, and the LVGL timer will handle the UI update.
        ESP_LOGI(TAG, "Button '%s' press handled in IMMEDIATE mode.", btn_name);
        // CORRECCIÓN: Forma segura de incrementar una variable volatile.
        immediate_press_counter = immediate_press_counter + 1;
    }
}

// Handler for the OK button (switches modes)
static void handle_ok_press() {
    if (current_mode == INPUT_DISPATCH_MODE_QUEUED) {
        current_mode = INPUT_DISPATCH_MODE_IMMEDIATE;
    } else {
        current_mode = INPUT_DISPATCH_MODE_QUEUED;
    }
    ESP_LOGI(TAG, "Switching to %s mode", (current_mode == INPUT_DISPATCH_MODE_QUEUED) ? "QUEUED" : "IMMEDIATE");
    
    // Apply the new mode to the button manager
    button_manager_set_dispatch_mode(current_mode);
    
    // Reset state and update UI
    immediate_press_counter = 0;
    update_ui_labels();
}

// Handler for the Cancel button (exits the view)
static void handle_cancel_press() {
    ESP_LOGI(TAG, "Exiting Button Test View.");
    // Clean up the timer
    if (ui_update_timer) {
        lv_timer_del(ui_update_timer);
        ui_update_timer = NULL;
    }
    // CRITICAL: Always restore the default mode for the rest of the application
    button_manager_set_dispatch_mode(INPUT_DISPATCH_MODE_QUEUED);
    // Go back to the main menu
    view_manager_load_view(VIEW_ID_MENU);
}

// Helper to update all static labels at once
static void update_ui_labels() {
    lv_label_set_text_fmt(mode_label, "Mode: %s", (current_mode == INPUT_DISPATCH_MODE_QUEUED) ? "QUEUED" : "IMMEDIATE");
    
    if (current_mode == INPUT_DISPATCH_MODE_QUEUED) {
        lv_label_set_text(last_press_label, "Last Queued Press: ---");
        lv_label_set_text(counter_label, "Immediate Count: N/A");
    } else {
        lv_label_set_text(last_press_label, "Last Queued Press: N/A");
        // CORRECCIÓN: Usar %lu para uint32_t
        lv_label_set_text_fmt(counter_label, "Immediate Count: %lu", immediate_press_counter);
    }
}

// Main function to create the view
void button_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Button Test View");

    // Start in a known state
    current_mode = INPUT_DISPATCH_MODE_QUEUED;
    immediate_press_counter = 0;
    button_manager_set_dispatch_mode(current_mode);

    // --- Create UI ---
    lv_obj_t *main_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(main_cont, 15, 0);

    lv_obj_t* title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "Button Dispatch Test");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    
    mode_label = lv_label_create(main_cont);
    lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_20, 0);

    last_press_label = lv_label_create(main_cont);
    counter_label = lv_label_create(main_cont);
    
    lv_obj_t* instructions_label = lv_label_create(main_cont);
    lv_label_set_text(instructions_label, "OK: Switch Mode\nCANCEL: Exit\nOther buttons: Test");
    lv_obj_set_style_text_align(instructions_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(instructions_label, 20, 0);

    // Set initial text on labels
    update_ui_labels();

    // --- Register Button Handlers ---
    button_manager_register_view_handler(BUTTON_OK, handle_ok_press);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press);
    button_manager_register_view_handler(BUTTON_LEFT, handle_test_button_left);
    button_manager_register_view_handler(BUTTON_RIGHT, handle_test_button_right);
    button_manager_register_view_handler(BUTTON_ON_OFF, handle_test_button_on_off);
    
    // --- Start UI Update Timer ---
    ui_update_timer = lv_timer_create(ui_update_timer_cb, 50, NULL);
}