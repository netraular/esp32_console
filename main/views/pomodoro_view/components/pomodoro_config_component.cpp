#include "pomodoro_config_component.h"
#include "controllers/button_manager/button_manager.h"
#include <vector>
#include "esp_log.h"

static const char* TAG = "POMO_CONFIG_COMP";

// --- Enums and State ---
typedef enum {
    FOCUS_WORK_MIN, FOCUS_WORK_SEC,
    FOCUS_BREAK_MIN, FOCUS_BREAK_SEC,
    FOCUS_ROUNDS,
    FOCUS_COUNT
} config_focus_t;

typedef struct {
    lv_obj_t* main_container;
    pomodoro_settings_t current_settings;
    config_focus_t focus;
    pomodoro_start_callback_t on_start_cb;
    pomodoro_exit_callback_t on_exit_cb;
    std::vector<lv_obj_t*> focusable_items;
} config_component_state_t;

// --- Function Prototypes ---
static void update_labels(config_component_state_t* state);
static void update_focus_highlight(config_component_state_t* state);
static void cleanup_event_cb(lv_event_t * e);

static const lv_color_t COLOR_BORDER_DEFAULT = lv_palette_main(LV_PALETTE_GREY);
static const lv_color_t COLOR_BORDER_FOCUSED = lv_palette_main(LV_PALETTE_YELLOW);
static const lv_color_t COLOR_BORDER_CONFIRMED = lv_color_hex(0x000000);


// --- Button Handlers ---
static void handle_ok_press(void* user_data) {
    auto* state = static_cast<config_component_state_t*>(user_data);

    if (state->focus == FOCUS_ROUNDS) {
        if (state->on_start_cb) {
            if (state->current_settings.work_seconds == 0) {
                state->current_settings.work_seconds = 1; // Prevent 0 second work time
            }
            state->on_start_cb(state->current_settings);
        }
    } else {
        state->focus = (config_focus_t)(((int)state->focus + 1));
        update_focus_highlight(state);
    }
}

static void handle_cancel_press(void* user_data) {
    auto* state = static_cast<config_component_state_t*>(user_data);

    if (state->focus == FOCUS_WORK_MIN) {
        if (state->on_exit_cb) {
            state->on_exit_cb();
        }
    } else {
        state->focus = (config_focus_t)(((int)state->focus - 1));
        update_focus_highlight(state);
    }
}

static void handle_left_press(void* user_data) {
    auto* state = static_cast<config_component_state_t*>(user_data);
    switch (state->focus) {
        case FOCUS_WORK_MIN: if (state->current_settings.work_seconds >= 60) state->current_settings.work_seconds -= 60; break;
        case FOCUS_WORK_SEC: state->current_settings.work_seconds = (state->current_settings.work_seconds % 60 > 0) ? state->current_settings.work_seconds - 1 : state->current_settings.work_seconds + 59; break;
        case FOCUS_BREAK_MIN: if (state->current_settings.break_seconds >= 60) state->current_settings.break_seconds -= 60; break;
        case FOCUS_BREAK_SEC: state->current_settings.break_seconds = (state->current_settings.break_seconds % 60 > 0) ? state->current_settings.break_seconds - 1 : state->current_settings.break_seconds + 59; break;
        case FOCUS_ROUNDS: if (state->current_settings.iterations > 1) state->current_settings.iterations--; break;
        default: break;
    }
    update_labels(state);
}

static void handle_right_press(void* user_data) {
    auto* state = static_cast<config_component_state_t*>(user_data);
    switch (state->focus) {
        case FOCUS_WORK_MIN: state->current_settings.work_seconds += 60; break;
        case FOCUS_WORK_SEC: state->current_settings.work_seconds = (state->current_settings.work_seconds % 60 < 59) ? state->current_settings.work_seconds + 1 : state->current_settings.work_seconds - 59; break;
        case FOCUS_BREAK_MIN: state->current_settings.break_seconds += 60; break;
        case FOCUS_BREAK_SEC: state->current_settings.break_seconds = (state->current_settings.break_seconds % 60 < 59) ? state->current_settings.break_seconds + 1 : state->current_settings.break_seconds - 59; break;
        case FOCUS_ROUNDS: state->current_settings.iterations++; break;
        default: break;
    }
    update_labels(state);
}

// --- UI Logic ---
static void update_labels(config_component_state_t* state) {
    lv_label_set_text_fmt(lv_obj_get_child(state->focusable_items[FOCUS_WORK_MIN], 0), "%02lu", state->current_settings.work_seconds / 60);
    lv_label_set_text_fmt(lv_obj_get_child(state->focusable_items[FOCUS_WORK_SEC], 0), "%02lu", state->current_settings.work_seconds % 60);
    lv_label_set_text_fmt(lv_obj_get_child(state->focusable_items[FOCUS_BREAK_MIN], 0), "%02lu", state->current_settings.break_seconds / 60);
    lv_label_set_text_fmt(lv_obj_get_child(state->focusable_items[FOCUS_BREAK_SEC], 0), "%02lu", state->current_settings.break_seconds % 60);
    lv_label_set_text_fmt(lv_obj_get_child(state->focusable_items[FOCUS_ROUNDS], 0), "%lu", state->current_settings.iterations);
}

static void update_focus_highlight(config_component_state_t* state) {
    config_focus_t current_focus = state->focus;

    for (int i = 0; i < FOCUS_COUNT; i++) {
        lv_obj_t* item = state->focusable_items[i];
        if (i < current_focus) {
            lv_obj_set_style_border_color(item, COLOR_BORDER_CONFIRMED, 0);
        } else if (i == current_focus) {
            lv_obj_set_style_border_color(item, COLOR_BORDER_FOCUSED, 0);
        } else {
            lv_obj_set_style_border_color(item, COLOR_BORDER_DEFAULT, 0);
        }
    }
}

static lv_obj_t* create_time_box(lv_obj_t* parent) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_set_size(box, 60, 40);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, COLOR_BORDER_DEFAULT, 0);
    lv_obj_t* label = lv_label_create(box);
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    return box;
}

lv_obj_t* pomodoro_config_component_create(lv_obj_t* parent, const pomodoro_settings_t initial_settings, pomodoro_start_callback_t on_start_cb, pomodoro_exit_callback_t on_exit_cb) {
    auto* state = new config_component_state_t;
    state->current_settings = initial_settings;
    state->on_start_cb = on_start_cb;
    state->on_exit_cb = on_exit_cb;
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
    lv_obj_set_width(work_row, lv_pct(100));
    lv_obj_set_height(work_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(work_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(work_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(work_row, 5, 0);
    lv_label_set_text(lv_label_create(work_row), "Work:");
    state->focusable_items[FOCUS_WORK_MIN] = create_time_box(work_row);
    lv_label_set_text(lv_label_create(work_row), ":");
    state->focusable_items[FOCUS_WORK_SEC] = create_time_box(work_row);

    // --- Break Row ---
    lv_obj_t* break_row = lv_obj_create(cont);
    lv_obj_remove_style_all(break_row);
    lv_obj_set_width(break_row, lv_pct(100));
    lv_obj_set_height(break_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(break_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(break_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(break_row, 5, 0);
    lv_label_set_text(lv_label_create(break_row), "Break:");
    state->focusable_items[FOCUS_BREAK_MIN] = create_time_box(break_row);
    lv_label_set_text(lv_label_create(break_row), ":");
    state->focusable_items[FOCUS_BREAK_SEC] = create_time_box(break_row);

    // --- Rounds Row ---
    lv_obj_t* rounds_row = lv_obj_create(cont);
    lv_obj_remove_style_all(rounds_row);
    lv_obj_set_width(rounds_row, lv_pct(100));
    lv_obj_set_height(rounds_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rounds_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rounds_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(rounds_row, 10, 0);
    lv_label_set_text(lv_label_create(rounds_row), "Rounds:");
    state->focusable_items[FOCUS_ROUNDS] = create_time_box(rounds_row);

    // Informational label
    lv_obj_t* info_label = lv_label_create(cont);
    lv_label_set_text(info_label, "OK: Next | Left/Right: Change\nCancel: Back / Exit");
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(info_label, lv_color_hex(0xaaaaaa), 0);

    update_labels(state);
    update_focus_highlight(state);

    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press, true, state);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press, true, state);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press, true, state);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, state);
    // Also handle long press for faster value changes
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_LONG_PRESS_HOLD, handle_left_press, true, state);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_HOLD, handle_right_press, true, state);

    return cont;
}

static void cleanup_event_cb(lv_event_t * e) {
    auto* state = static_cast<config_component_state_t*>(lv_event_get_user_data(e));
    if (state) {
        ESP_LOGI(TAG, "Cleaning up Pomodoro config component");
        delete state;
    }
}