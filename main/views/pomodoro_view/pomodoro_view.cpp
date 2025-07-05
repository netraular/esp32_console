#include "pomodoro_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "components/pomodoro_config_component.h"
#include "components/pomodoro_timer_component.h"
#include "esp_log.h"

static const char* TAG = "POMODORO_VIEW";

// --- State Management for the Main View ---
typedef enum {
    POMODORO_STATE_CONFIG,
    POMODORO_STATE_RUNNING
} pomodoro_main_state_t;

static struct {
    lv_obj_t* parent_container;
    lv_obj_t* current_component;
    pomodoro_main_state_t state;
    pomodoro_settings_t last_settings;
} s_view_state;


// --- Forward Declarations of Callbacks and State Changers ---
static void on_start_pressed(const pomodoro_settings_t settings);
static void on_timer_exit();
static void handle_cancel_press_main(void* user_data);
static void show_config_screen();
static void show_timer_screen(const pomodoro_settings_t settings);


// --- Main Logic: Switching between components ---
static void show_config_screen() {
    if (s_view_state.current_component) {
        lv_obj_del(s_view_state.current_component);
    }
    s_view_state.current_component = pomodoro_config_component_create(s_view_state.parent_container, s_view_state.last_settings, on_start_pressed);
    s_view_state.state = POMODORO_STATE_CONFIG;
    // Register the main "exit view" handler. The component will register its own handlers for other buttons.
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_main, true, nullptr);
}

static void show_timer_screen(const pomodoro_settings_t settings) {
    if (s_view_state.current_component) {
        lv_obj_del(s_view_state.current_component);
    }
    s_view_state.current_component = pomodoro_timer_component_create(s_view_state.parent_container, settings, on_timer_exit);
    s_view_state.state = POMODORO_STATE_RUNNING;
    // The timer component will register its own handlers, including for the CANCEL button.
}


// --- Callbacks passed to components ---
static void on_start_pressed(const pomodoro_settings_t settings) {
    ESP_LOGI(TAG, "Start pressed. Work: %lu, Break: %lu, Rounds: %lu", settings.work_seconds, settings.break_seconds, settings.iterations);
    s_view_state.last_settings = settings; // Save settings for next time
    show_timer_screen(settings);
}

static void on_timer_exit() {
    ESP_LOGI(TAG, "Timer exited. Returning to config screen.");
    show_config_screen();
}

// --- Main button handler (only for exiting the whole view) ---
static void handle_cancel_press_main(void* user_data) {
    // This handler is only active on the config screen.
    ESP_LOGI(TAG, "Exiting Pomodoro view.");
    if (s_view_state.current_component) {
        lv_obj_del(s_view_state.current_component);
        s_view_state.current_component = nullptr;
    }
    view_manager_load_view(VIEW_ID_MENU);
}


// --- Public Entry Point ---
void pomodoro_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Pomodoro main view");

    s_view_state.parent_container = parent;
    s_view_state.current_component = nullptr;
    
    // Set default initial settings
    s_view_state.last_settings.work_seconds = 25 * 60; // 25 minutes
    s_view_state.last_settings.break_seconds = 5 * 60;   // 5 minutes
    s_view_state.last_settings.iterations = 4;

    show_config_screen();
}