#include "notification_manager.h"
#include "esp_log.h"
#include <algorithm>
#include <memory>
#include "views/view_manager.h"
#include "components/popup_manager/popup_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/littlefs_manager/littlefs_manager.h"
#include "views/core/standby_view/standby_view.h"
#include "json_generator.h"
#include "json_parser.h"
#include "jsmn.h"
#include <sys/stat.h>

static const char *TAG = "NOTIF_MGR";
static const char* DATA_DIR = "data";
static const char* NOTIFICATIONS_FILE_PATH = "data/notifications.json";
static const char* NOTIFICATION_SOUND_PATH = "/sdcard/sounds/notification.wav";

std::vector<Notification> NotificationManager::s_notifications;
uint32_t NotificationManager::s_next_id = 1;
lv_timer_t* NotificationManager::s_dispatcher_timer = nullptr;
bool NotificationManager::s_wakeup_sound_pending = false;
static uint32_t s_dispatcher_paused_until = 0; // Local to this file

void NotificationManager::init() {
    s_notifications.clear();
    s_next_id = 1;
    s_wakeup_sound_pending = false;
    s_dispatcher_paused_until = 0;
    if (littlefs_manager_ensure_dir_exists(DATA_DIR)) {
        load_notifications();
    } else {
        ESP_LOGE(TAG, "Failed to create data directory, notifications will not be persistent.");
    }
    s_dispatcher_timer = lv_timer_create(dispatcher_task, 1000, nullptr);
    lv_timer_ready(s_dispatcher_timer);
    ESP_LOGI(TAG, "Notification Manager initialized and dispatcher started.");
}

void NotificationManager::dispatcher_task(lv_timer_t* timer) {
    // --- 1. Handle Wake-up Sound Action ---
    if (s_wakeup_sound_pending) {
        s_wakeup_sound_pending = false; // Consume the event
        ESP_LOGI(TAG, "Dispatcher playing wake-up notification sound.");
        if (sd_manager_check_ready()) {
            struct stat st;
            if (stat(NOTIFICATION_SOUND_PATH, &st) == 0) {
                audio_manager_play(NOTIFICATION_SOUND_PATH);
            } else {
                ESP_LOGW(TAG, "Notification sound file not found at %s", NOTIFICATION_SOUND_PATH);
            }
        } else {
            ESP_LOGW(TAG, "SD card not ready, cannot play notification sound.");
        }
        // After a wake-up, pause the visual popup dispatcher to prevent race conditions.
        s_dispatcher_paused_until = esp_log_timestamp() + 2000; // Pause for 2 seconds
    }

    // --- 2. Handle Visual Popup Action ---
    // Check if the visual dispatcher is currently paused
    if (esp_log_timestamp() < s_dispatcher_paused_until) {
        return;
    }

    // Only dispatch popups if the user is on the standby screen and no other popup is active.
    if (view_manager_get_current_view_id() != VIEW_ID_STANDBY || popup_manager_is_active()) {
        return;
    }

    time_t now = time(NULL);
    // Defines a 1-second window to catch notifications.
    time_t one_second_ago = now - 1;

    auto it = std::find_if(s_notifications.begin(), s_notifications.end(), 
        [&](const Notification& notif) {
            return !notif.is_read && notif.timestamp <= now && notif.timestamp > one_second_ago;
        });

    if (it != s_notifications.end()) {
        Notification& notif_to_show = *it;
        ESP_LOGI(TAG, "Dispatching visual notification popup for ID: %lu to StandbyView", notif_to_show.id);
        
        StandbyView::show_notification_popup(notif_to_show);
        
        mark_as_read(notif_to_show.id);
    }
}

void NotificationManager::handle_wakeup_event() {
    s_wakeup_sound_pending = true;
    ESP_LOGI(TAG, "Wake-up event received. Sound playback is pending for the next dispatcher cycle.");
}

time_t NotificationManager::get_next_notification_timestamp() {
    time_t now = time(NULL);
    time_t next_timestamp = 0;

    for (const auto& notif : s_notifications) {
        if (!notif.is_read && notif.timestamp > now) {
            if (next_timestamp == 0 || notif.timestamp < next_timestamp) {
                next_timestamp = notif.timestamp;
            }
        }
    }
    
    if (next_timestamp > 0) {
        ESP_LOGD(TAG, "Next notification is at timestamp %ld", (long)next_timestamp);
    }
    
    return next_timestamp;
}

uint32_t NotificationManager::get_next_unique_id() { return s_next_id++; }

void NotificationManager::add_notification(const std::string& title, const std::string& message, time_t timestamp) {
    Notification new_notif;
    new_notif.id = get_next_unique_id();
    new_notif.title = title;
    new_notif.message = message;
    new_notif.timestamp = timestamp;
    new_notif.is_read = false;
    
    s_notifications.push_back(new_notif);
    ESP_LOGI(TAG, "Added new notification (ID: %lu, Timestamp: %ld): '%s'", new_notif.id, (long)new_notif.timestamp, new_notif.title.c_str());
    save_notifications();
}

std::vector<Notification> NotificationManager::get_unread_notifications() {
    std::vector<Notification> unread;
    time_t now = time(NULL);
    for (const auto& notif : s_notifications) {
        if (!notif.is_read && notif.timestamp <= now) {
            unread.push_back(notif);
        }
    }
    std::sort(unread.begin(), unread.end(), [](const Notification& a, const Notification& b) {
        return a.timestamp > b.timestamp;
    });
    return unread;
}

std::vector<Notification> NotificationManager::get_pending_notifications() {
    std::vector<Notification> pending;
    time_t now = time(NULL);
    for (const auto& notif : s_notifications) {
        if (notif.timestamp > now) {
            pending.push_back(notif);
        }
    }
    std::sort(pending.begin(), pending.end(), [](const Notification& a, const Notification& b) {
        return a.timestamp < b.timestamp;
    });
    return pending;
}

void NotificationManager::mark_as_read(uint32_t id) {
    auto it = std::find_if(s_notifications.begin(), s_notifications.end(), 
                           [id](const Notification& notif) { return notif.id == id; });
    if (it != s_notifications.end()) {
        if (!it->is_read) {
            it->is_read = true;
            ESP_LOGI(TAG, "Marked notification (ID: %lu) as read.", id);
            save_notifications();
        }
    } else {
        ESP_LOGW(TAG, "Attempted to mark non-existent notification (ID: %lu) as read.", id);
    }
}

void NotificationManager::clear_all_notifications() {
    s_notifications.clear();
    ESP_LOGI(TAG, "All notifications cleared.");
    save_notifications();
}

void NotificationManager::save_notifications() {
    const size_t buffer_size = 4096;
    auto json_buffer = std::make_unique<char[]>(buffer_size);
    if (!json_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON buffer. Cannot save notifications.");
        return;
    }
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, json_buffer.get(), buffer_size, NULL, NULL);
    json_gen_start_array(&jstr);
    for (const auto& notif : s_notifications) {
        json_gen_start_object(&jstr);
        json_gen_obj_set_int(&jstr, "id", notif.id);
        json_gen_obj_set_string(&jstr, "title", notif.title.c_str());
        json_gen_obj_set_string(&jstr, "message", notif.message.c_str());
        json_gen_obj_set_int(&jstr, "timestamp", notif.timestamp);
        json_gen_obj_set_bool(&jstr, "is_read", notif.is_read);
        json_gen_end_object(&jstr);
    }
    json_gen_end_array(&jstr);
    json_gen_str_end(&jstr);
    if (littlefs_manager_write_file(NOTIFICATIONS_FILE_PATH, json_buffer.get())) {
        ESP_LOGD(TAG, "Successfully saved %d notifications to LittleFS.", s_notifications.size());
    } else {
        ESP_LOGE(TAG, "Failed to save notifications to LittleFS.");
    }
}

void NotificationManager::load_notifications() {
    char* file_buffer = nullptr;
    size_t file_size = 0;
    if (!littlefs_manager_read_file(NOTIFICATIONS_FILE_PATH, &file_buffer, &file_size)) {
        ESP_LOGI(TAG, "No notifications file found. Starting fresh.");
        if(file_buffer) free(file_buffer);
        return;
    }
    ESP_LOGD(TAG, "Found notifications file, parsing...");
    jparse_ctx_t jctx;
    int ret = json_parse_start(&jctx, file_buffer, file_size);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse JSON file: %s", esp_err_to_name(ret));
        free(file_buffer);
        return;
    }
    if (jctx.tokens[0].type != JSMN_ARRAY) {
        ESP_LOGE(TAG, "Expected a JSON array as the root object.");
        json_parse_end(&jctx);
        free(file_buffer);
        return;
    }
    uint32_t max_id = 0;
    s_notifications.clear();
    int num_elements = jctx.tokens[0].size;
    for (int i = 0; i < num_elements; i++) {
        if (json_arr_get_object(&jctx, i) == 0) {
            Notification temp_notif;
            long long temp_ll;
            char temp_str_buf[256];
            if (json_obj_get_int64(&jctx, "id", &temp_ll) == 0) temp_notif.id = (uint32_t)temp_ll;
            if (json_obj_get_int64(&jctx, "timestamp", &temp_ll) == 0) temp_notif.timestamp = (time_t)temp_ll;
            if (json_obj_get_bool(&jctx, "is_read", &temp_notif.is_read) != 0) temp_notif.is_read = false;
            if (json_obj_get_string(&jctx, "title", temp_str_buf, sizeof(temp_str_buf)) >= 0) temp_notif.title = temp_str_buf;
            if (json_obj_get_string(&jctx, "message", temp_str_buf, sizeof(temp_str_buf)) >= 0) temp_notif.message = temp_str_buf;
            s_notifications.push_back(temp_notif);
            if (temp_notif.id > max_id) {
                max_id = temp_notif.id;
            }
            json_arr_leave_object(&jctx);
        }
    }
    s_next_id = max_id + 1;
    json_parse_end(&jctx);
    free(file_buffer);
    ESP_LOGI(TAG, "Loaded %d notifications. Next ID is %lu.", s_notifications.size(), s_next_id);
}