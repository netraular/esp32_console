#include "pomodoro_view.h"
#include "views/view_manager.h"
#include "controllers/daily_summary_manager/daily_summary_manager.h"
#include "components/pomodoro_config_component.h"
#include "components/pomodoro_timer_component.h"
#include "esp_log.h"
#include <time.h>

static const char* TAG = "POMODORO_VIEW";

// Initialize the static instance pointer
PomodoroView* PomodoroView::s_instance = nullptr;

// --- Lifecycle Methods ---
PomodoroView::PomodoroView() {
    ESP_LOGI(TAG, "PomodoroView constructed");
    s_instance = this; // Set the singleton instance

    // Set default initial settings
    last_settings.work_seconds = 25 * 60; // 25 minutes
    last_settings.break_seconds = 5 * 60;   // 5 minutes
    last_settings.iterations = 4;
}

PomodoroView::~PomodoroView() {
    ESP_LOGI(TAG, "PomodoroView destructed");
    // When the view is destroyed, the ViewManager calls lv_obj_clean on the
    // parent screen. This will delete our 'container' object. Since
    // 'current_component' is a child of 'container', LVGL will automatically
    // delete it, triggering its own LV_EVENT_DELETE cleanup callback.
    // We just need to clear our static pointer.
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void PomodoroView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Pomodoro View");
    // Use the base class 'container' as the main holder for this view.
    // This container is managed (and deleted) by the ViewManager.
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_center(container);
    
    // Start with the configuration screen
    show_config_screen();
}

// --- View State Changers ---
void PomodoroView::show_config_screen() {
    if (current_component) {
        lv_obj_del(current_component);
        current_component = nullptr;
    }

    current_component = pomodoro_config_component_create(
        container, 
        last_settings, 
        start_pressed_cb_c, // Callback to start the timer
        config_exit_cb_c    // Callback to exit the view
    );
    current_state = STATE_CONFIG;
}

void PomodoroView::show_timer_screen(const pomodoro_settings_t& settings) {
    if (current_component) {
        lv_obj_del(current_component);
        current_component = nullptr;
    }

    current_component = pomodoro_timer_component_create(
        container, 
        settings, 
        timer_exit_cb_c, // Callback to return to config
        work_session_complete_cb_c // Callback for reporting completed work
    );
    current_state = STATE_RUNNING;
}

// --- Instance Methods for Callbacks ---
void PomodoroView::on_start_pressed(const pomodoro_settings_t& settings) {
    ESP_LOGI(TAG, "Start pressed. Work: %lu, Break: %lu, Rounds: %lu", 
             settings.work_seconds, settings.break_seconds, settings.iterations);
    last_settings = settings; // Save settings for next time
    show_timer_screen(settings);
}

void PomodoroView::on_config_exit() {
    ESP_LOGI(TAG, "Config screen exit requested. Returning to menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

void PomodoroView::on_timer_exit() {
    ESP_LOGI(TAG, "Timer exited. Returning to config screen.");
    show_config_screen();
}

void PomodoroView::on_work_session_complete(uint32_t seconds) {
    ESP_LOGI(TAG, "Work session completed for %lu seconds. Saving to daily summary.", seconds);
    DailySummaryManager::add_pomodoro_work_time(time(NULL), seconds);
}


// --- Static Callback Bridges ---
void PomodoroView::start_pressed_cb_c(const pomodoro_settings_t settings) {
    if (s_instance) {
        s_instance->on_start_pressed(settings);
    }
}

void PomodoroView::config_exit_cb_c() {
    if (s_instance) {
        s_instance->on_config_exit();
    }
}

void PomodoroView::timer_exit_cb_c() {
    if (s_instance) {
        s_instance->on_timer_exit();
    }
}

void PomodoroView::work_session_complete_cb_c(uint32_t seconds) {
    if (s_instance) {
        s_instance->on_work_session_complete(seconds);
    }
}