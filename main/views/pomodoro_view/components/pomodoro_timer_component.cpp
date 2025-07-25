#include "pomodoro_timer_component.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"

static const char* TAG = "POMO_TIMER_COMP";

// --- Enums and State ---
typedef enum {
    POMO_STATUS_RUNNING,
    POMO_STATUS_PAUSED,
    POMO_STATUS_FINISHED
} pomodoro_status_t;

typedef enum {
    POMO_MODE_WORK,
    POMO_MODE_BREAK
} pomodoro_mode_t;

typedef struct {
    lv_obj_t* main_container;
    lv_obj_t* time_label;
    lv_obj_t* status_label;
    lv_obj_t* iteration_label;
    lv_obj_t* progress_arc;
    lv_timer_t* timer;
    pomodoro_exit_callback_t on_exit_cb;
    pomodoro_status_t status;
    pomodoro_mode_t mode;
    pomodoro_settings_t settings;
    uint32_t remaining_seconds;
    uint32_t current_iteration;
} timer_component_state_t;

// --- Prototypes ---
static void timer_update_cb(lv_timer_t *timer);
static void cleanup_event_cb(lv_event_t * e);
static void start_next_mode(timer_component_state_t* state);

// --- Button Handlers ---
static void handle_ok_press(void* user_data) {
    auto* state = static_cast<timer_component_state_t*>(user_data);
    if (state->status == POMO_STATUS_RUNNING) {
        state->status = POMO_STATUS_PAUSED;
        lv_timer_pause(state->timer);
        lv_label_set_text(state->status_label, "PAUSED");
    } else if (state->status == POMO_STATUS_PAUSED) {
        state->status = POMO_STATUS_RUNNING;
        lv_timer_resume(state->timer);
        lv_label_set_text(state->status_label, state->mode == POMO_MODE_WORK ? "WORK" : "BREAK");
    }
}

static void handle_cancel_press(void* user_data) {
    auto* state = static_cast<timer_component_state_t*>(user_data);
    if (state->on_exit_cb) {
        state->on_exit_cb();
    }
}

// --- Helper function to switch between WORK and BREAK modes ---
static void start_next_mode(timer_component_state_t* state) {
    if (state->mode == POMO_MODE_WORK) {
        // --- Transition from WORK to BREAK ---
        state->mode = POMO_MODE_BREAK;
        state->remaining_seconds = state->settings.break_seconds;
        lv_label_set_text(state->status_label, "BREAK");
        lv_obj_set_style_arc_color(state->progress_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    } else { 
        // --- Transition from BREAK to WORK ---
        state->current_iteration++;
        if (state->current_iteration > state->settings.iterations) {
            // --- All rounds finished ---
            state->status = POMO_STATUS_FINISHED;
            lv_timer_pause(state->timer);
            lv_label_set_text(state->status_label, "FINISHED!");
            lv_obj_add_flag(state->time_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(state->iteration_label, "Press Cancel to exit");
            return;
        }
        state->mode = POMO_MODE_WORK;
        state->remaining_seconds = state->settings.work_seconds;
        lv_label_set_text(state->status_label, "WORK");
        lv_label_set_text_fmt(state->iteration_label, "Round: %lu / %lu", state->current_iteration, state->settings.iterations);
        lv_obj_set_style_arc_color(state->progress_arc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    }

    uint32_t new_total_seconds = (state->mode == POMO_MODE_WORK) ? state->settings.work_seconds : state->settings.break_seconds;
    if (new_total_seconds == 0) new_total_seconds = 1; // Prevent division by zero if time is 0
    lv_arc_set_range(state->progress_arc, 0, new_total_seconds);
    lv_arc_set_value(state->progress_arc, 0);
}

// --- Main Timer Callback ---
static void timer_update_cb(lv_timer_t *timer) {
    auto* state = static_cast<timer_component_state_t*>(lv_timer_get_user_data(timer));

    if (state->remaining_seconds > 0) {
        state->remaining_seconds--;
    }
    
    // Update display labels and arc
    uint32_t total_seconds_in_mode = (state->mode == POMO_MODE_WORK) ? state->settings.work_seconds : state->settings.break_seconds;
    lv_label_set_text_fmt(state->time_label, "%02lu:%02lu", state->remaining_seconds / 60, state->remaining_seconds % 60);
    lv_arc_set_value(state->progress_arc, total_seconds_in_mode - state->remaining_seconds);

    // Check if the current mode has finished
    if (state->remaining_seconds == 0) {
        start_next_mode(state);
    }
}

// --- Cleanup ---
static void cleanup_event_cb(lv_event_t * e) {
    auto* state = static_cast<timer_component_state_t*>(lv_event_get_user_data(e));
    if (state) {
        ESP_LOGI(TAG, "Cleaning up Pomodoro timer component.");
        if (state->timer) {
            lv_timer_delete(state->timer);
            state->timer = nullptr;
        }
        delete state;
    }
}

// --- Public Entry Point ---
lv_obj_t* pomodoro_timer_component_create(lv_obj_t* parent, const pomodoro_settings_t settings, pomodoro_exit_callback_t on_exit_cb) {
    auto* state = new timer_component_state_t;
    state->settings = settings;
    state->on_exit_cb = on_exit_cb;
    state->status = POMO_STATUS_RUNNING;
    state->mode = POMO_MODE_WORK;
    state->current_iteration = 1;
    state->remaining_seconds = settings.work_seconds > 0 ? settings.work_seconds : 1;

    state->main_container = lv_obj_create(parent);
    lv_obj_remove_style_all(state->main_container);
    lv_obj_set_size(state->main_container, lv_pct(100), lv_pct(100));
    lv_obj_center(state->main_container);
    lv_obj_add_event_cb(state->main_container, cleanup_event_cb, LV_EVENT_DELETE, state);

    state->progress_arc = lv_arc_create(state->main_container);
    lv_arc_set_rotation(state->progress_arc, 270);
    lv_arc_set_bg_angles(state->progress_arc, 0, 360);
    lv_obj_set_size(state->progress_arc, 200, 200);
    lv_obj_center(state->progress_arc);
    lv_obj_remove_flag(state->progress_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_range(state->progress_arc, 0, state->remaining_seconds);
    lv_arc_set_value(state->progress_arc, 0);
    lv_obj_set_style_arc_color(state->progress_arc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(state->progress_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(state->progress_arc, 10, LV_PART_INDICATOR);
    
    state->time_label = lv_label_create(state->main_container);
    lv_obj_set_style_text_font(state->time_label, &lv_font_montserrat_48, 0);
    lv_obj_center(state->time_label);

    state->status_label = lv_label_create(state->main_container);
    lv_label_set_text(state->status_label, "WORK");
    lv_obj_set_style_text_font(state->status_label, &lv_font_montserrat_24, 0);
    lv_obj_align(state->status_label, LV_ALIGN_CENTER, 0, -55);

    state->iteration_label = lv_label_create(state->main_container);
    lv_label_set_text_fmt(state->iteration_label, "Round: %lu / %lu", state->current_iteration, state->settings.iterations);
    lv_obj_align(state->iteration_label, LV_ALIGN_CENTER, 0, 55);

    state->timer = lv_timer_create(timer_update_cb, 1000, state);
    timer_update_cb(state->timer); // Initial update to show correct time immediately

    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press, true, state);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, state);
    
    return state->main_container;
}