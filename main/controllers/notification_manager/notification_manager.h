#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include "models/notification_data_model.h"
#include <vector>
#include <string>
#include "lvgl.h" 
#include "components/popup_manager/popup_manager.h"

class NotificationManager {
public:
    static void init();
    
    /**
     * @brief Adds a notification to the manager with a specific timestamp.
     * @param title The title of the notification.
     * @param message The body of the notification.
     * @param timestamp The Unix timestamp when the notification should occur.
     */
    static void add_notification(const std::string& title, const std::string& message, time_t timestamp);
    
    /**
     * @brief Gets the Unix timestamp of the next scheduled (pending) notification.
     * @return The timestamp of the next notification, or 0 if there are no pending notifications.
     */
    static time_t get_next_notification_timestamp();

    /**
     * @brief Informs the manager that a wake-up event occurred, likely for a notification.
     * This will trigger a sound playback on the next dispatcher cycle.
     */
    static void handle_wakeup_event();

    static std::vector<Notification> get_unread_notifications();
    static std::vector<Notification> get_pending_notifications();
    static void mark_as_read(uint32_t id);
    static void clear_all_notifications();

private:
    static std::vector<Notification> s_notifications;
    static uint32_t s_next_id;
    static lv_timer_t* s_dispatcher_timer;
    static bool s_wakeup_sound_pending; // Flag to play sound on next cycle

    static uint32_t get_next_unique_id();
    static void dispatcher_task(lv_timer_t* timer); 

    // --- Persistence ---
    static void load_notifications();
    static void save_notifications();
};

#endif // NOTIFICATION_MANAGER_H