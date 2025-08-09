#include "notification_manager.h"
#include "models/asset_config.h" // Use the centralized asset configuration
#include "esp_log.h"
#include <algorithm>
#include <memory>
#include <string>
#include "cJSON.h" 
#include "views/view_manager.h"
#include "components/popup_manager/popup_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/littlefs_manager/littlefs_manager.h"
#include "views/core/standby_view/standby_view.h"
#include <sys/stat.h>

static const char *TAG = "NOTIF_MGR";

// --- File paths derived entirely from the central asset_config.h ---
static const std::string s_notifications_dir_path = std::string(USER_DATA_BASE_PATH) + NOTIFICATIONS_SUBPATH;
static const std::string s_notifications_filepath = s_notifications_dir_path + NOTIFICATIONS_FILENAME;
static const std::string s_notifications_temp_filepath = s_notifications_dir_path + NOTIFICATIONS_TEMP_FILENAME;

// --- Static members ---
std::vector<Notification> NotificationManager::s_notifications;
uint32_t NotificationManager::s_next_id = 1;
lv_timer_t* NotificationManager::s_dispatcher_timer = nullptr;

void NotificationManager::init() {
    s_notifications.clear();
    s_next_id = 1;
    
    if (littlefs_manager_ensure_dir_exists(s_notifications_dir_path.c_str())) {
        load_notifications();
    } else {
        ESP_LOGE(TAG, "Failed to create notifications directory, notifications will not be persistent.");
    }
    
    s_dispatcher_timer = lv_timer_create(dispatcher_task, 1000, nullptr);
    lv_timer_ready(s_dispatcher_timer);
    ESP_LOGI(TAG, "Notification Manager initialized and dispatcher started.");
}

void NotificationManager::save_notifications() {
    cJSON *root = cJSON_CreateArray();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create cJSON array.");
        return;
    }

    for (const auto& notif : s_notifications) {
        cJSON *notif_json = cJSON_CreateObject();
        // ... (JSON creation logic is unchanged)
        cJSON_AddNumberToObject(notif_json, "id", notif.id);
        cJSON_AddStringToObject(notif_json, "title", notif.title.c_str());
        cJSON_AddStringToObject(notif_json, "message", notif.message.c_str());
        cJSON_AddNumberToObject(notif_json, "timestamp", notif.timestamp);
        cJSON_AddBoolToObject(notif_json, "is_read", notif.is_read);
        cJSON_AddItemToArray(root, notif_json);
    }

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_string) {
        ESP_LOGE(TAG, "Failed to print cJSON to string.");
        return;
    }

    // Atomic Write Implementation using central paths
    if (!littlefs_manager_write_file(s_notifications_temp_filepath.c_str(), json_string)) {
        ESP_LOGE(TAG, "Failed to write to temporary notifications file.");
        free(json_string);
        return;
    }

    if (littlefs_manager_file_exists(s_notifications_filepath.c_str())) {
        if (!littlefs_manager_delete_file(s_notifications_filepath.c_str())) {
             ESP_LOGE(TAG, "Failed to delete old notifications file. Aborting atomic save.");
             littlefs_manager_delete_file(s_notifications_temp_filepath.c_str());
             free(json_string);
             return;
        }
    }

    if (!littlefs_manager_rename_file(s_notifications_temp_filepath.c_str(), s_notifications_filepath.c_str())) {
        ESP_LOGE(TAG, "CRITICAL: Failed to rename temp notifications file. Data may be in '.tmp' file!");
    } else {
        ESP_LOGD(TAG, "Successfully saved %d notifications to LittleFS.", s_notifications.size());
    }

    free(json_string);
}

void NotificationManager::load_notifications() {
    // Atomic Load/Recovery using central paths
    if (littlefs_manager_file_exists(s_notifications_temp_filepath.c_str())) {
        ESP_LOGW(TAG, "Found temporary notifications file, indicating an incomplete write.");
        if (littlefs_manager_rename_file(s_notifications_temp_filepath.c_str(), s_notifications_filepath.c_str())) {
            ESP_LOGI(TAG, "Successfully restored from temporary file.");
        } else {
            ESP_LOGE(TAG, "Failed to restore from temp file. Deleting temp file.");
            littlefs_manager_delete_file(s_notifications_temp_filepath.c_str());
        }
    }
    
    char* file_buffer = nullptr;
    size_t file_size = 0;
    if (!littlefs_manager_read_file(s_notifications_filepath.c_str(), &file_buffer, &file_size) || file_size == 0) {
        ESP_LOGI(TAG, "No valid notifications file found. Starting fresh.");
        if(file_buffer) free(file_buffer);
        return;
    }
    
    // ... (JSON parsing logic is unchanged) ...
    ESP_LOGD(TAG, "Found notifications file, parsing...");
    cJSON *root = cJSON_ParseWithLength(file_buffer, file_size);
    
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Failed to parse JSON file. Error near: %s", error_ptr);
        } else {
            ESP_LOGE(TAG, "Failed to parse JSON file. Unknown error.");
        }
        free(file_buffer);
        return;
    }
    
    free(file_buffer);

    if (!cJSON_IsArray(root)) {
        ESP_LOGE(TAG, "JSON root is not an array.");
        cJSON_Delete(root);
        return;
    }

    s_notifications.clear();
    uint32_t max_id = 0;

    cJSON *notif_json = NULL;
    cJSON_ArrayForEach(notif_json, root) {
        Notification temp_notif = {};
        cJSON *id_json = cJSON_GetObjectItem(notif_json, "id");
        cJSON *title_json = cJSON_GetObjectItem(notif_json, "title");
        cJSON *message_json = cJSON_GetObjectItem(notif_json, "message");
        cJSON *timestamp_json = cJSON_GetObjectItem(notif_json, "timestamp");
        cJSON *is_read_json = cJSON_GetObjectItem(notif_json, "is_read");

        if (cJSON_IsNumber(id_json)) temp_notif.id = id_json->valueint;
        if (cJSON_IsString(title_json)) temp_notif.title = title_json->valuestring;
        if (cJSON_IsString(message_json)) temp_notif.message = message_json->valuestring;
        if (cJSON_IsNumber(timestamp_json)) temp_notif.timestamp = (time_t)timestamp_json->valuedouble;
        if (cJSON_IsBool(is_read_json)) temp_notif.is_read = cJSON_IsTrue(is_read_json);

        s_notifications.push_back(temp_notif);
        if (temp_notif.id > max_id) {
            max_id = temp_notif.id;
        }
    }
    
    s_next_id = max_id + 1;
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Loaded %d notifications. Next ID is %lu.", s_notifications.size(), s_next_id);
}

// ... (Rest of the public API methods remain unchanged) ...
void NotificationManager::dispatcher_task(lv_timer_t* timer) {
    if (view_manager_get_current_view_id() != VIEW_ID_STANDBY || popup_manager_is_active()) {
        return;
    }

    time_t now = time(NULL);
    time_t one_second_ago = now - 1;

    auto it = std::find_if(s_notifications.begin(), s_notifications.end(), 
        [&](const Notification& notif) {
            return !notif.is_read && notif.timestamp <= now && notif.timestamp > one_second_ago;
        });

    if (it != s_notifications.end()) {
        Notification& notif_to_show = *it;
        ESP_LOGI(TAG, "Dispatching visual notification for ID: %lu to StandbyView", notif_to_show.id);
        
        StandbyView::show_notification_popup(notif_to_show);
        
        if (sd_manager_check_ready()) {
            char sound_path[256];
            snprintf(sound_path, sizeof(sound_path), "%s%s%s%s%s",
                     SD_CARD_ROOT_PATH,
                     ASSETS_BASE_SUBPATH,
                     ASSETS_SOUNDS_SUBPATH,
                     SOUNDS_EFFECTS_SUBPATH,
                     UI_SOUND_NOTIFICATION);

            struct stat st;
            if (stat(sound_path, &st) == 0) {
                audio_manager_play(sound_path);
            } else {
                ESP_LOGW(TAG, "Notification sound file not found at %s", sound_path);
            }
        }

        mark_as_read(notif_to_show.id);
    }
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
    Notification new_notif = {
        .id = get_next_unique_id(),
        .title = title,
        .message = message,
        .timestamp = timestamp,
        .is_read = false
    };
    
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