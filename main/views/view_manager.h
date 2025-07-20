#ifndef VIEW_MANAGER_H
#define VIEW_MANAGER_H

#include "lvgl.h"

// Enum to uniquely identify each view
typedef enum {
    VIEW_ID_STANDBY,
    // Add other views here as they are converted
    VIEW_ID_COUNT // Total number of views
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