#ifndef VIEW_MANAGER_H
#define VIEW_MANAGER_H

#include "lvgl.h"

// Enum to uniquely identify each view
typedef enum {
    VIEW_ID_MENU,
    VIEW_ID_MIC_TEST,
    VIEW_ID_SPEAKER_TEST,
    VIEW_ID_SD_TEST,
    VIEW_ID_IMAGE_TEST,
    VIEW_ID_BUZZER_TEST,
    VIEW_ID_COUNT // Total number of views
} view_id_t;

/**
 * @brief Initializes the view manager and loads the initial view.
 */
void view_manager_init(void);

/**
 * @brief Loads a new view.
 * This function destroys the current view, cleans the screen, and creates the new one.
 * @param view_id The ID of the view to load.
 */
void view_manager_load_view(view_id_t view_id);

#endif // VIEW_MANAGER_H