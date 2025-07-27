#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include "models/notification_data_model.h"
#include <vector>
#include <string>

/**
 * @brief Manages the lifecycle of system notifications.
 *
 * This class acts as a centralized service for creating, storing, and retrieving
 * notifications. It maintains a queue of notifications and is responsible for
 * triggering their display under the correct conditions.
 */
class NotificationManager {
public:
    /**
     * @brief Initializes the Notification Manager.
     * Must be called once at application startup.
     */
    static void init();

    /**
     * @brief Adds a new notification to the queue.
     * This is the primary method for other system components to create a notification.
     * @param title The title for the notification popup.
     * @param message The main text content for the notification.
     */
    static void add_notification(const std::string& title, const std::string& message);

    /**
     * @brief Retrieves a list of all unread notifications.
     * @return A vector containing all notifications where is_read is false.
     */
    static std::vector<Notification> get_unread_notifications();

    /**
     * @brief Marks a specific notification as read.
     * This should be called after a user has interacted with/dismissed a notification.
     * @param id The unique ID of the notification to mark as read.
     */
    static void mark_as_read(uint32_t id);

    /**
     * @brief Deletes all notifications from the system.
     */
    static void clear_all_notifications();

private:
    // In-memory cache of all notifications
    static std::vector<Notification> s_notifications;
    static uint32_t s_next_id;

    // TODO (Task 5): Implement persistence to LittleFS
    // static void load_from_fs();
    // static void save_to_fs();

    static uint32_t get_next_unique_id();
};

#endif // NOTIFICATION_MANAGER_H