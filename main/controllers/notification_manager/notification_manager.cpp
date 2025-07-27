#include "notification_manager.h"
#include "esp_log.h"
#include <algorithm>
#include "views/view_manager.h"                  // <-- ADDED
#include "components/popup_manager/popup_manager.h"// <-- ADDED
#include "controllers/audio_manager/audio_manager.h" // <-- ADDED

static const char *TAG = "NOTIF_MGR";

// --- Static Member Initialization ---
std::vector<Notification> NotificationManager::s_notifications;
uint32_t NotificationManager::s_next_id = 1;
lv_timer_t* NotificationManager::s_dispatcher_timer = nullptr;

void NotificationManager::init() {
    // For now, we just clear the in-memory vector on init.
    // In a future task, this will be replaced with loading from LittleFS.
    s_notifications.clear();
    s_next_id = 1;

    // Create the dispatcher timer to run every second
    s_dispatcher_timer = lv_timer_create(dispatcher_task, 1000, nullptr);
    lv_timer_ready(s_dispatcher_timer);

    ESP_LOGI(TAG, "Notification Manager initialized (in-memory) and dispatcher started.");
}

void NotificationManager::dispatcher_task(lv_timer_t* timer) {
    // Condition 1: Only show popups in the Standby view
    if (view_manager_get_current_view_id() != VIEW_ID_STANDBY) {
        return;
    }

    // Condition 2: Do not show a notification if another popup is already active
    if (popup_manager_is_active()) {
        return;
    }

    // Condition 3: Check if there are unread notifications
    auto it = std::find_if(s_notifications.begin(), s_notifications.end(), 
                           [](const Notification& notif) {
                               return !notif.is_read;
                           });

    if (it == s_notifications.end()) {
        return; // No unread notifications
    }
    
    // All conditions met, show the popup for the first unread notification
    Notification& notif_to_show = *it;
    
    ESP_LOGI(TAG, "Dispatching notification popup for ID: %lu", notif_to_show.id);

    // Play a sound
    // Note: Ensure this sound file exists on your SD card or LittleFS
    audio_manager_play("/sdcard/sounds/notification.wav");

    // The user_data must be heap-allocated because the popup manager doesn't
    // manage its lifecycle, and the notification object could be invalidated
    // if the vector resizes. We will free it in the callback.
    uint32_t* notif_id_ptr = (uint32_t*)malloc(sizeof(uint32_t));
    if(notif_id_ptr) {
        *notif_id_ptr = notif_to_show.id;
        popup_manager_show_alert(notif_to_show.title.c_str(), 
                                 notif_to_show.message.c_str(), 
                                 on_popup_closed, 
                                 notif_id_ptr);
        
        // Mark as read immediately to prevent the dispatcher from trying
        // to show it again in the next tick before the popup is fully created.
        notif_to_show.is_read = true;
    } else {
        ESP_LOGE(TAG, "Failed to allocate memory for notification ID pointer!");
    }
}

void NotificationManager::on_popup_closed(popup_result_t result, void* user_data) {
    uint32_t* notif_id_ptr = (uint32_t*)user_data;
    if (notif_id_ptr) {
        ESP_LOGD(TAG, "Popup for notification ID %lu closed.", *notif_id_ptr);
        // The notification was already marked as read when the popup was created.
        // We just need to free the memory we allocated for the ID.
        free(notif_id_ptr);
    }

    // IMPORTANT: The popup manager unregisters view handlers. The StandbyView
    // needs to re-register its own handlers. Since the notification manager is
    // generic, it can't know about StandbyView.
    // The StandbyView will need to handle this. For now, this is a known issue.
    // A better solution would be an event bus, but for now, the StandbyView will
    // need to periodically check and re-register its handlers if needed.
    // (We will address this implicitly when the popup callback re-registers handlers in the calling view)
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