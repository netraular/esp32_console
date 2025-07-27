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
    static void add_notification(const std::string& title, const std::string& message);
    static std::vector<Notification> get_unread_notifications();
    static void mark_as_read(uint32_t id);
    static void clear_all_notifications();

private:
    static std::vector<Notification> s_notifications;
    static uint32_t s_next_id;
    static lv_timer_t* s_dispatcher_timer;
    static uint32_t get_next_unique_id();
    static void dispatcher_task(lv_timer_t* timer); 
    // REMOVED on_popup_closed
};

#endif // NOTIFICATION_MANAGER_H