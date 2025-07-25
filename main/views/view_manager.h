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

// Enum to uniquely identify each view
typedef enum {
    // Core Views
    VIEW_ID_STANDBY,
    VIEW_ID_MENU,

    // Test Views from the menu
    VIEW_ID_MIC_TEST,
    VIEW_ID_SPEAKER_TEST,
    VIEW_ID_SD_TEST,
    VIEW_ID_IMAGE_TEST,
    VIEW_ID_LITTLEFS_TEST,
    VIEW_ID_MULTI_CLICK_TEST,
    VIEW_ID_WIFI_STREAM_TEST,
    VIEW_ID_POMODORO,
    VIEW_ID_CLICK_COUNTER_TEST,
    VIEW_ID_VOICE_NOTE,
    VIEW_ID_VOICE_NOTE_PLAYER,
    VIEW_ID_VOLUME_TESTER,
    
    // Habit Tracker Views
    VIEW_ID_HABIT_MANAGER,
    VIEW_ID_HABIT_CATEGORY_MANAGER,
    VIEW_ID_HABIT_ADD,
    VIEW_ID_TRACK_HABITS,
    
    // Add other views here as they are converted
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

#endif // VIEW_MANAGER_H