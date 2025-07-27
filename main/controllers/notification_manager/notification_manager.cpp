#include "notification_manager.h"
#include "esp_log.h"
#include <algorithm> // For std::find_if

static const char *TAG = "NOTIF_MGR";

// --- Static Member Initialization ---
std::vector<Notification> NotificationManager::s_notifications;
uint32_t NotificationManager::s_next_id = 1;

void NotificationManager::init() {
    // For now, we just clear the in-memory vector on init.
    // In a future task, this will be replaced with loading from LittleFS.
    s_notifications.clear();
    s_next_id = 1;
    ESP_LOGI(TAG, "Notification Manager initialized (in-memory).");
}

uint32_t NotificationManager::get_next_unique_id() {
    // In a more robust system, we would check for ID rollover or load the last ID from disk.
    // For this simple implementation, a static counter is sufficient.
    return s_next_id++;
}

void NotificationManager::add_notification(const std::string& title, const std::string& message) {
    Notification new_notif;
    new_notif.id = get_next_unique_id();
    new_notif.title = title;
    new_notif.message = message;
    new_notif.timestamp = time(NULL);
    new_notif.is_read = false;

    s_notifications.push_back(new_notif);
    ESP_LOGI(TAG, "Added new notification (ID: %lu): '%s'", new_notif.id, new_notif.title.c_str());

    // TODO (Task 5): Call save_to_fs() here.
}

std::vector<Notification> NotificationManager::get_unread_notifications() {
    std::vector<Notification> unread;
    for (const auto& notif : s_notifications) {
        if (!notif.is_read) {
            unread.push_back(notif);
        }
    }
    return unread;
}

void NotificationManager::mark_as_read(uint32_t id) {
    auto it = std::find_if(s_notifications.begin(), s_notifications.end(), 
                           [id](const Notification& notif) {
                               return notif.id == id;
                           });

    if (it != s_notifications.end()) {
        if (!it->is_read) {
            it->is_read = true;
            ESP_LOGI(TAG, "Marked notification (ID: %lu) as read.", id);
            // TODO (Task 5): Call save_to_fs() here.
        }
    } else {
        ESP_LOGW(TAG, "Attempted to mark non-existent notification (ID: %lu) as read.", id);
    }
}

void NotificationManager::clear_all_notifications() {
    s_notifications.clear();
    ESP_LOGI(TAG, "All notifications cleared.");
    // TODO (Task 5): Also delete the file from LittleFS.
}