#ifndef POMODORO_VIEW_H
#define POMODORO_VIEW_H

#include "../view.h"
#include "lvgl.h"
#include "components/pomodoro_common.h"

// Forward declarations to avoid including component headers here
struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

/**
 * @brief Manages the Pomodoro feature, switching between configuration and timer components.
 *
 * This class encapsulates the entire Pomodoro view logic. It acts as a controller
 * that creates and destroys the child components (config screen, timer screen)
 * based on the user's actions.
 */
class PomodoroView : public View {
public:
    PomodoroView();
    ~PomodoroView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- State ---
    enum PomodoroState {
        STATE_CONFIG,
        STATE_RUNNING
    };

    PomodoroState current_state = STATE_CONFIG;
    pomodoro_settings_t last_settings;
    lv_obj_t* current_component = nullptr;

    // --- Singleton-like instance for C-style callbacks from components ---
    // A workaround for C components that don't support a `user_data` context pointer.
    static PomodoroView* s_instance;

    // --- Private Methods ---
    void show_config_screen();
    void show_timer_screen(const pomodoro_settings_t& settings);

    // Instance methods called by static callbacks
    void on_start_pressed(const pomodoro_settings_t& settings);
    void on_config_exit();
    void on_timer_exit();

    // --- Static Callback Bridges ---
    // These functions are passed to the C-style components and bridge
    // the call to the active C++ class instance.
    static void start_pressed_cb_c(const pomodoro_settings_t settings);
    static void config_exit_cb_c();
    static void timer_exit_cb_c();
};

#endif // POMODORO_VIEW_H