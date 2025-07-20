#include "multi_click_test_view.h"
#include "esp_log.h"
#include <map>

static const char *TAG = "MULTI_CLICK_TEST_VIEW";

// --- Static Member Initialization ---
const char* MultiClickTestView::button_names[BUTTON_COUNT] = { "Left", "Cancel", "OK", "Right", "On/Off" };

// Maps our event enum to a user-friendly string for display
static const std::map<button_event_type_t, const char*> event_type_to_string = {
    {BUTTON_EVENT_PRESS_DOWN,       "Press Down"},
    {BUTTON_EVENT_PRESS_UP,         "Press Up"},
    {BUTTON_EVENT_TAP,              "Tap (Fast)"},
    {BUTTON_EVENT_SINGLE_CLICK,     "Single Click"},
    {BUTTON_EVENT_DOUBLE_CLICK,     "Double Click"},
    {BUTTON_EVENT_LONG_PRESS_START, "Long Press Start"},
    {BUTTON_EVENT_LONG_PRESS_HOLD,  "Long Press Hold"},
};

// --- Lifecycle Methods ---
MultiClickTestView::MultiClickTestView() {
    ESP_LOGI(TAG, "MultiClickTestView constructed");
}

MultiClickTestView::~MultiClickTestView() {
    ESP_LOGI(TAG, "MultiClickTestView destructed");
    // No dynamic resources to clean up.
}

void MultiClickTestView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Multi-Click Test View UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);
    
    setup_ui(container);
    setup_button_handlers();
}

// --- UI Setup ---
void MultiClickTestView::setup_ui(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_set_style_pad_gap(parent, 8, 0);

    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Button Event Test");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_margin_bottom(title_label, 10, 0);

    static lv_coord_t col_dsc[] = {80, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_size(grid, lv_pct(100), LV_SIZE_CONTENT);
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

    lv_obj_t* instructions_label = lv_label_create(parent);
    lv_label_set_text(instructions_label, "Press CANCEL to exit");
    lv_obj_set_style_margin_top(instructions_label, 15, 0);
}

// --- Button Handling ---
void MultiClickTestView::setup_button_handlers() {
    // Register the exit handler for the Cancel button
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, MultiClickTestView::cancel_press_cb, true, this);

    // Register a generic handler for all other events on all other buttons
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        // Skip the cancel button, as it has a dedicated purpose
        if (static_cast<button_id_t>(i) == BUTTON_CANCEL) continue;

        for (auto const& [event_type, event_name] : event_type_to_string) {
            // Populate the context struct for this specific button and event
            contexts[i][event_type] = {this, static_cast<button_id_t>(i), event_type};
            // Register the handler, passing a pointer to our context struct
            button_manager_register_handler(static_cast<button_id_t>(i), event_type, MultiClickTestView::generic_event_cb, true, &contexts[i][event_type]);
        }
    }
}

// --- Instance Methods ---
void MultiClickTestView::handle_event(button_id_t button, const char* event_name) {
    if (button < BUTTON_COUNT && event_labels[button]) {
        ESP_LOGI(TAG, "Button '%s' Event: %s", button_names[button], event_name);
        lv_label_set_text(event_labels[button], event_name);
    }
}

void MultiClickTestView::on_event(button_id_t button, button_event_type_t event_type) {
    auto it = event_type_to_string.find(event_type);
    if (it != event_type_to_string.end()) {
        handle_event(button, it->second);
    }
}

void MultiClickTestView::on_cancel_press() {
    ESP_LOGI(TAG, "Exiting view.");
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks ---
void MultiClickTestView::generic_event_cb(void* user_data) {
    event_context* ctx = static_cast<event_context*>(user_data);
    if (ctx && ctx->view_instance) {
        ctx->view_instance->on_event(ctx->button_id, ctx->event_type);
    }
}

void MultiClickTestView::cancel_press_cb(void* user_data) {
    static_cast<MultiClickTestView*>(user_data)->on_cancel_press();
}