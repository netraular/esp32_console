#include "notification_manager.h"
#include "esp_log.h"
#include <algorithm>
#include "views/view_manager.h"
#include "components/popup_manager/popup_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "views/standby_view/standby_view.h" // <-- ADDED: To call StandbyView's function

static const char *TAG = "NOTIF_MGR";

// --- Static Member Initialization ---
std::vector<Notification> NotificationManager::s_notifications;
uint32_t NotificationManager::s_next_id = 1;
lv_timer_t* NotificationManager::s_dispatcher_timer = nullptr;

void NotificationManager::init() {
    s_notifications.clear();
    s_next_id = 1;
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
    
    // All conditions met, delegate showing the popup to the StandbyView
    Notification& notif_to_show = *it;
    
    ESP_LOGI(TAG, "Dispatching notification popup for ID: %lu to StandbyView", notif_to_show.id);

    // Call the static method on StandbyView to handle the UI part
    StandbyView::show_notification_popup(notif_to_show);
    
    // Mark as read immediately to prevent the dispatcher from trying
    // to show it again in the next tick before the popup is fully created.
    // The StandbyView will get a reference, so this change will be visible.
    notif_to_show.is_read = true;
}

// REMOVED on_popup_closed, as StandbyView will handle it now.

uint32_t NotificationManager::get_next_unique_id() {
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
        }
    } else {
        ESP_LOGW(TAG, "Attempted to mark non-existent notification (ID: %lu) as read.", id);
    }
}

void NotificationManager::clear_all_notifications() {
    s_notifications.clear();
    ESP_LOGI(TAG, "All notifications cleared.");
}