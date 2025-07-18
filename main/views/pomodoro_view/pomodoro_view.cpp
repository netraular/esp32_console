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

// This struct holds the state for the pomodoro view, including a pointer to the
// container that holds all UI elements for this view.
static struct {
    lv_obj_t* view_container;
    lv_obj_t* current_component;
    pomodoro_main_state_t state;
    pomodoro_settings_t last_settings;
} s_view_state;


// --- Forward Declarations of Callbacks and State Changers ---
static void on_start_pressed(const pomodoro_settings_t settings);
static void on_timer_exit();
static void show_config_screen();
static void show_timer_screen(const pomodoro_settings_t settings);
static void cleanup_pomodoro_view();

// --- Callback to clean up view resources on deletion ---
static void cleanup_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Pomodoro view container is being deleted, cleaning up resources.");
        cleanup_pomodoro_view();
    }
}

// --- Centralized cleanup function ---
static void cleanup_pomodoro_view() {
    // This function is called when the view's main container is deleted.
    // The key cleanup action is handled automatically by LVGL: when `view_container` is
    // deleted, all of its children (like `s_view_state.current_component`) are also deleted.
    // This triggers the LV_EVENT_DELETE callbacks on the components themselves,
    // which are responsible for freeing their own specific resources (e.g., timers, state).
    ESP_LOGD(TAG, "Child components will be deleted by LVGL's parent-child mechanism.");
    s_view_state.current_component = nullptr; // Avoid dangling pointer.
    // If this view itself allocated any timers or tasks, they would be deleted here.
}


// --- Main Logic: Switching between components ---
static void show_config_screen() {
    if (s_view_state.current_component) {
        lv_obj_del(s_view_state.current_component);
        s_view_state.current_component = nullptr;
    }
    s_view_state.current_component = pomodoro_config_component_create(s_view_state.view_container, s_view_state.last_settings, on_start_pressed);
    s_view_state.state = POMODORO_STATE_CONFIG;
}

static void show_timer_screen(const pomodoro_settings_t settings) {
    if (s_view_state.current_component) {
        lv_obj_del(s_view_state.current_component);
        s_view_state.current_component = nullptr;
    }
    s_view_state.current_component = pomodoro_timer_component_create(s_view_state.view_container, settings, on_timer_exit);
    s_view_state.state = POMODORO_STATE_RUNNING;
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


// --- Public Entry Point ---
void pomodoro_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Pomodoro main view");

    // Create a dedicated container for this view. This is crucial for proper cleanup.
    // When view_manager calls lv_obj_clean(), this container will be deleted,
    // which in turn will trigger its LV_EVENT_DELETE event.
    s_view_state.view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(s_view_state.view_container);
    lv_obj_set_size(s_view_state.view_container, lv_pct(100), lv_pct(100));
    lv_obj_center(s_view_state.view_container);

    // Add the cleanup callback to the view's own container.
    lv_obj_add_event_cb(s_view_state.view_container, cleanup_event_cb, LV_EVENT_DELETE, NULL);

    s_view_state.current_component = nullptr;
    
    // Set default initial settings
    s_view_state.last_settings.work_seconds = 25 * 60; // 25 minutes
    s_view_state.last_settings.break_seconds = 5 * 60;   // 5 minutes
    s_view_state.last_settings.iterations = 4;

    show_config_screen();
}