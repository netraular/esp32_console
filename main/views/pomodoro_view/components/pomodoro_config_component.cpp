#include "pomodoro_config_component.h"
#include "controllers/button_manager/button_manager.h"
#include <vector>

// --- Enums and State ---
typedef enum {
    FOCUS_WORK_MIN, FOCUS_WORK_SEC,
    FOCUS_BREAK_MIN, FOCUS_BREAK_SEC,
    FOCUS_ROUNDS,
    FOCUS_START_BTN,
    FOCUS_COUNT
} config_focus_t;

typedef struct {
    lv_obj_t* main_container;
    pomodoro_settings_t current_settings;
    config_focus_t focus;
    pomodoro_start_callback_t on_start_cb;
    std::vector<lv_obj_t*> focusable_items;
} config_component_state_t;

// --- Function Prototypes ---
static void update_labels(config_component_state_t* state);
static void update_focus_highlight(config_component_state_t* state, config_focus_t old_focus, config_focus_t new_focus);
static void cleanup_event_cb(lv_event_t * e);

// --- Button Handlers ---
static void handle_ok_press(void* user_data) {
    auto* state = static_cast<config_component_state_t*>(user_data);
    if (state->focus == FOCUS_START_BTN) {
        if (state->on_start_cb) {
            // Ensure at least 1 second of work to prevent issues
            if (state->current_settings.work_seconds == 0) {
                state->current_settings.work_seconds = 1;
            }
            state->on_start_cb(state->current_settings);
        }
    } else {
        config_focus_t old_focus = state->focus;
        state->focus = (config_focus_t)(((int)state->focus + 1) % FOCUS_COUNT);
        update_focus_highlight(state, old_focus, state->focus);
    }
}

static void handle_left_press(void* user_data) {
    auto* state = static_cast<config_component_state_t*>(user_data);
    switch (state->focus) {
        case FOCUS_WORK_MIN: if (state->current_settings.work_seconds >= 60) state->current_settings.work_seconds -= 60; break;
        case FOCUS_WORK_SEC: if (state->current_settings.work_seconds % 60 > 0) state->current_settings.work_seconds--; else state->current_settings.work_seconds += 59; break;
        case FOCUS_BREAK_MIN: if (state->current_settings.break_seconds >= 60) state->current_settings.break_seconds -= 60; break;
        case FOCUS_BREAK_SEC: if (state->current_settings.break_seconds % 60 > 0) state->current_settings.break_seconds--; else state->current_settings.break_seconds += 59; break;
        case FOCUS_ROUNDS: if (state->current_settings.iterations > 1) state->current_settings.iterations--; break;
        default: break;
    }
    update_labels(state);
}

static void handle_right_press(void* user_data) {
    auto* state = static_cast<config_component_state_t*>(user_data);
    switch (state->focus) {
        case FOCUS_WORK_MIN: state->current_settings.work_seconds += 60; break;
        case FOCUS_WORK_SEC: if (state->current_settings.work_seconds % 60 < 59) state->current_settings.work_seconds++; else state->current_settings.work_seconds -= 59; break;
        case FOCUS_BREAK_MIN: state->current_settings.break_seconds += 60; break;
        case FOCUS_BREAK_SEC: if (state->current_settings.break_seconds % 60 < 59) state->current_settings.break_seconds++; else state->current_settings.break_seconds -= 59; break;
        case FOCUS_ROUNDS: state->current_settings.iterations++; break;
        default: break;
    }
    update_labels(state);
}

// --- UI Logic ---
void update_labels(config_component_state_t* state) {
    lv_label_set_text_fmt(lv_obj_get_child(state->focusable_items[FOCUS_WORK_MIN], 0), "%02lu", state->current_settings.work_seconds / 60);
    lv_label_set_text_fmt(lv_obj_get_child(state->focusable_items[FOCUS_WORK_SEC], 0), "%02lu", state->current_settings.work_seconds % 60);
    lv_label_set_text_fmt(lv_obj_get_child(state->focusable_items[FOCUS_BREAK_MIN], 0), "%02lu", state->current_settings.break_seconds / 60);
    lv_label_set_text_fmt(lv_obj_get_child(state->focusable_items[FOCUS_BREAK_SEC], 0), "%02lu", state->current_settings.break_seconds % 60);
    lv_label_set_text_fmt(lv_obj_get_child(state->focusable_items[FOCUS_ROUNDS], 0), "%lu", state->current_settings.iterations);
}

void update_focus_highlight(config_component_state_t* state, config_focus_t old_focus, config_focus_t new_focus) {
    lv_obj_set_style_border_color(state->focusable_items[old_focus], lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_color(state->focusable_items[new_focus], lv_palette_main(LV_PALETTE_YELLOW), 0);
}

lv_obj_t* create_time_box(lv_obj_t* parent) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_set_size(box, 60, 40);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_pad_all(box, 5, 0);
    lv_obj_t* label = lv_label_create(box);
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    return box;
}

lv_obj_t* pomodoro_config_component_create(lv_obj_t* parent, const pomodoro_settings_t initial_settings, pomodoro_start_callback_t on_start_cb) {
    auto* state = new config_component_state_t;
    state->current_settings = initial_settings;
    state->on_start_cb = on_start_cb;
    state->focus = FOCUS_WORK_MIN;
    state->focusable_items.resize(FOCUS_COUNT);

    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(cont, cleanup_event_cb, LV_EVENT_DELETE, state);
    state->main_container = cont;

    // --- Work Row ---
    lv_obj_t* work_row = lv_obj_create(cont);
    lv_obj_remove_style_all(work_row);
    lv_obj_set_flex_flow(work_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(work_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // CORRECCIÓN: Usar la sintaxis de C para la API de LVGL
    lv_label_set_text(lv_label_create(work_row), "Work: ");
    state->focusable_items[FOCUS_WORK_MIN] = create_time_box(work_row);
    lv_label_set_text(lv_label_create(work_row), ":");
    state->focusable_items[FOCUS_WORK_SEC] = create_time_box(work_row);

    // --- Break Row ---
    lv_obj_t* break_row = lv_obj_create(cont);
    lv_obj_remove_style_all(break_row);
    lv_obj_set_flex_flow(break_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(break_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_label_set_text(lv_label_create(break_row), "Break:");
    state->focusable_items[FOCUS_BREAK_MIN] = create_time_box(break_row);
    lv_label_set_text(lv_label_create(break_row), ":");
    state->focusable_items[FOCUS_BREAK_SEC] = create_time_box(break_row);

    // --- Rounds Row ---
    lv_obj_t* rounds_row = lv_obj_create(cont);
    lv_obj_remove_style_all(rounds_row);
    lv_obj_set_flex_flow(rounds_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rounds_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_label_set_text(lv_label_create(rounds_row), "Rounds:");
    state->focusable_items[FOCUS_ROUNDS] = create_time_box(rounds_row);

    // --- Start Button ---
    lv_obj_t* start_btn = lv_button_create(cont);
    lv_label_set_text(lv_label_create(start_btn), "START");
    state->focusable_items[FOCUS_START_BTN] = start_btn;
    lv_obj_set_style_border_width(start_btn, 2, 0);
    lv_obj_set_style_border_color(start_btn, lv_color_hex(0x444444), 0);

    update_labels(state);
    update_focus_highlight(state, FOCUS_START_BTN, state->focus);

    // Register button handlers
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press, true, state);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press, true, state);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press, true, state);
    // Cancel is handled by the parent view

    return cont;
}

static void cleanup_event_cb(lv_event_t * e) {
    auto* state = static_cast<config_component_state_t*>(lv_event_get_user_data(e));
    if (state) {
        button_manager_unregister_view_handlers();
        delete state;
    }
}