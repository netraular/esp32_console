/**
 * @file view_manager.h
 * @brief Manages the application's UI views and transitions between them.
 *
 * This manager uses a factory pattern to create and destroy view objects,
 * ensuring clean state transitions and resource management. It handles the
 * lifecycle of each view, from creation to destruction.
 */
#ifndef VIEW_MANAGER_H
#define VIEW_MANAGER_H

#include "lvgl.h"

// X-Macro to define the list of all views. This is the single source of truth.
// To add a new view, add its name here without the "VIEW_ID_" prefix.
#define FOR_EACH_VIEW(V) \
    /* Core Views */ \
    V(STANDBY) \
    V(MENU) \
    V(SETTINGS) \
    /* Game/Gamification Views */ \
    V(PET_VIEW) \
    /* Test Views */ \
    V(MIC_TEST) \
    V(SPEAKER_TEST) \
    V(SD_TEST) \
    V(IMAGE_TEST) \
    V(LITTLEFS_TEST) \
    V(MULTI_CLICK_TEST) \
    V(WIFI_STREAM_TEST) \
    V(POMODORO) \
    V(CLICK_COUNTER_TEST) \
    V(VOICE_NOTE) \
    V(VOICE_NOTE_PLAYER) \
    V(VOLUME_TESTER) \
    V(POPUP_TEST) \
    /* Habit Tracker Views */ \
    V(HABIT_MANAGER) \
    V(HABIT_CATEGORY_MANAGER) \
    V(HABIT_ADD) \
    V(TRACK_HABITS) \
    V(HABIT_HISTORY) \
    /* Notification System Views */ \
    V(ADD_NOTIFICATION) \
    V(NOTIFICATION_HISTORY)

// Macro to generate enum members from the list
#define GENERATE_ENUM(NAME) VIEW_ID_##NAME,

/**
 * @brief Enum to uniquely identify each view.
 * This is automatically generated from the FOR_EACH_VIEW macro.
 */
typedef enum {
    FOR_EACH_VIEW(GENERATE_ENUM)
    VIEW_ID_COUNT // Total number of views, must be last
} view_id_t;

/**
 * @brief Initializes the view manager and loads the initial view.
 */
void view_manager_init(void);

/**
 * @brief Loads a new view.
 * This function destroys the current view, cleaning up all its resources
 * automatically via its destructor, and then creates the new one.
 * @param view_id The ID of the view to load.
 */
void view_manager_load_view(view_id_t view_id);

/**
 * @brief Gets the ID of the currently active view.
 * @return The view_id_t of the current view.
 */
view_id_t view_manager_get_current_view_id(void);

/**
 * @brief Gets the string name of a view from its ID.
 * @param view_id The ID of the view.
 * @return A constant string pointer to the view's name, or "UNKNOWN_VIEW" if invalid.
 */
const char* view_manager_get_view_name(view_id_t view_id);

#endif // VIEW_MANAGER_H