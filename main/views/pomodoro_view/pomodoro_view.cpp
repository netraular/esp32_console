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
static void show_config_screen();
static void show_timer_screen(const pomodoro_settings_t settings);
static void cleanup_pomodoro_view();

// --- Callback to clean up view resources on deletion ---
static void cleanup_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Pomodoro view is being deleted, cleaning up resources.");
        cleanup_pomodoro_view();
    }
}

// --- Centralized cleanup function ---
static void cleanup_pomodoro_view() {
    // The components are children of parent_container. When the parent is deleted
    // by view_manager, this callback is fired. lv_obj_del will be called on the children,
    // which in turn should trigger their own cleanup events, destroying any internal timers.
    // This assumes the child components are also well-behaved and clean up on LV_EVENT_DELETE.
    if (s_view_state.current_component) {
        // No need to call lv_obj_del here if the parent is being deleted,
        // but this makes explicit cleanup clearer.
        s_view_state.current_component = nullptr;
    }
    // If this view itself created any timers or tasks, they would be deleted here.
}


// --- Main Logic: Switching between components ---
static void show_config_screen() {
    if (s_view_state.current_component) {
        lv_obj_del(s_view_state.current_component);
        s_view_state.current_component = nullptr;
    }
    s_view_state.current_component = pomodoro_config_component_create(s_view_state.parent_container, s_view_state.last_settings, on_start_pressed);
    s_view_state.state = POMODORO_STATE_CONFIG;
}

static void show_timer_screen(const pomodoro_settings_t settings) {
    if (s_view_state.current_component) {
        lv_obj_del(s_view_state.current_component);
        s_view_state.current_component = nullptr;
    }
    s_view_state.current_component = pomodoro_timer_component_create(s_view_state.parent_container, settings, on_timer_exit);
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

    s_view_state.parent_container = parent;
    s_view_state.current_component = nullptr;
    
    // Set default initial settings
    s_view_state.last_settings.work_seconds = 25 * 60; // 25 minutes
    s_view_state.last_settings.break_seconds = 5 * 60;   // 5 minutes
    s_view_state.last_settings.iterations = 4;
    
    // Add the cleanup callback to the main container for this view.
    // When the view_manager cleans the screen, this will be called automatically.
    lv_obj_add_event_cb(parent, cleanup_event_cb, LV_EVENT_DELETE, NULL);

    show_config_screen();
}