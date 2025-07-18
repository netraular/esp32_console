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

// --- FIX: Encapsulated state for the view ---
// This struct holds the state for the pomodoro view. It's allocated dynamically.
typedef struct {
    lv_obj_t* view_container;
    lv_obj_t* current_component;
    pomodoro_main_state_t state;
    pomodoro_settings_t last_settings;
} pomodoro_view_state_t;

// --- Module-scope pointer to the current view's state ---
// This is necessary because C-style callbacks cannot be class members.
static pomodoro_view_state_t* s_view_state_ptr = nullptr;

// --- Forward Declarations of Callbacks and State Changers ---
static void on_start_pressed(const pomodoro_settings_t settings);
static void on_timer_exit();
static void show_config_screen();
static void show_timer_screen(const pomodoro_settings_t settings);
static void cleanup_event_cb(lv_event_t *e);


// --- Main Logic: Switching between components ---
static void show_config_screen() {
    if (!s_view_state_ptr) return;

    if (s_view_state_ptr->current_component) {
        lv_obj_del(s_view_state_ptr->current_component);
        s_view_state_ptr->current_component = nullptr;
    }
    s_view_state_ptr->current_component = pomodoro_config_component_create(
        s_view_state_ptr->view_container, 
        s_view_state_ptr->last_settings, 
        on_start_pressed
    );
    s_view_state_ptr->state = POMODORO_STATE_CONFIG;
}

static void show_timer_screen(const pomodoro_settings_t settings) {
    if (!s_view_state_ptr) return;

    if (s_view_state_ptr->current_component) {
        lv_obj_del(s_view_state_ptr->current_component);
        s_view_state_ptr->current_component = nullptr;
    }
    s_view_state_ptr->current_component = pomodoro_timer_component_create(
        s_view_state_ptr->view_container, 
        settings, 
        on_timer_exit
    );
    s_view_state_ptr->state = POMODORO_STATE_RUNNING;
}

// --- Callbacks passed to components ---
static void on_start_pressed(const pomodoro_settings_t settings) {
    if (!s_view_state_ptr) return;
    ESP_LOGI(TAG, "Start pressed. Work: %lu, Break: %lu, Rounds: %lu", settings.work_seconds, settings.break_seconds, settings.iterations);
    s_view_state_ptr->last_settings = settings; // Save settings for next time
    show_timer_screen(settings);
}

static void on_timer_exit() {
    if (!s_view_state_ptr) return;
    ESP_LOGI(TAG, "Timer exited. Returning to config screen.");
    show_config_screen();
}

// --- FIX: Robust Cleanup using LV_EVENT_DELETE ---
static void cleanup_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Pomodoro view container is being deleted, cleaning up its state.");
        
        // This view's direct components (current_component) are children of the 
        // container and will be deleted automatically by LVGL, triggering their
        // own cleanup callbacks. We just need to clean up this view's own state.
        if (s_view_state_ptr) {
            delete s_view_state_ptr;
            s_view_state_ptr = nullptr;
        }
    }
}

// --- Public Entry Point ---
void pomodoro_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Pomodoro main view");

    // Allocate the state for this view instance.
    s_view_state_ptr = new pomodoro_view_state_t;

    // Create a dedicated container for this view. This is crucial for proper cleanup.
    s_view_state_ptr->view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(s_view_state_ptr->view_container);
    lv_obj_set_size(s_view_state_ptr->view_container, lv_pct(100), lv_pct(100));
    lv_obj_center(s_view_state_ptr->view_container);

    // Add the cleanup callback to the view's own container.
    lv_obj_add_event_cb(s_view_state_ptr->view_container, cleanup_event_cb, LV_EVENT_DELETE, NULL);

    s_view_state_ptr->current_component = nullptr;
    
    // Set default initial settings
    s_view_state_ptr->last_settings.work_seconds = 25 * 60; // 25 minutes
    s_view_state_ptr->last_settings.break_seconds = 5 * 60;   // 5 minutes
    s_view_state_ptr->last_settings.iterations = 4;

    show_config_screen();
}