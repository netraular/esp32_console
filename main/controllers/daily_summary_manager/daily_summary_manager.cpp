#include "daily_summary_manager.h"
#include "controllers/littlefs_manager/littlefs_manager.h"
#include "models/asset_config.h"
#include "esp_log.h"
#include "cJSON.h"
#include <dirent.h>
#include <algorithm>
#include <cstring> // FIX: Added for strcmp

static const char* TAG = "DAILY_SUMMARY_MGR";

// --- Private Helper Functions ---

static std::string get_summary_dir_path() {
    return std::string(littlefs_manager_get_mount_point()) + "/" + USER_DATA_BASE_PATH + SUMMARY_SUBPATH;
}

time_t DailySummaryManager::get_start_of_day(time_t timestamp) {
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    return mktime(&timeinfo);
}

std::string DailySummaryManager::get_filepath_for_date(time_t date) {
    time_t start_of_day = get_start_of_day(date);
    struct tm timeinfo;
    localtime_r(&start_of_day, &timeinfo);

    char filename_buf[32];
    strftime(filename_buf, sizeof(filename_buf), "%Y%m%d.json", &timeinfo);
    
    return std::string(USER_DATA_BASE_PATH) + SUMMARY_SUBPATH + filename_buf;
}

bool DailySummaryManager::save_summary(const DailySummaryData& summary) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create cJSON root object.");
        return false;
    }

    cJSON_AddNumberToObject(root, "date", summary.date);
    cJSON_AddStringToObject(root, "journal_path", summary.journal_entry_path.c_str());

    cJSON *habits = cJSON_CreateArray();
    for (uint32_t id : summary.completed_habit_ids) {
        cJSON_AddItemToArray(habits, cJSON_CreateNumber(id));
    }
    cJSON_AddItemToObject(root, "completed_habit_ids", habits);

    cJSON *notes = cJSON_CreateArray();
    for (const auto& path : summary.voice_note_paths) {
        cJSON_AddItemToArray(notes, cJSON_CreateString(path.c_str()));
    }
    cJSON_AddItemToObject(root, "voice_note_paths", notes);

    char *json_string = cJSON_PrintUnformatted(root);
    std::string filepath = get_filepath_for_date(summary.date);
    bool success = littlefs_manager_write_file(filepath.c_str(), json_string);

    free(json_string);
    cJSON_Delete(root);

    if (!success) {
        ESP_LOGE(TAG, "Failed to write summary file: %s", filepath.c_str());
    }
    return success;
}

// --- Public API Implementation ---

void DailySummaryManager::init() {
    ESP_LOGI(TAG, "Initializing Daily Summary Manager...");
    std::string path = std::string(USER_DATA_BASE_PATH) + SUMMARY_SUBPATH;
    if (!littlefs_manager_ensure_dir_exists(path.c_str())) {
        ESP_LOGE(TAG, "Failed to create daily summary directory! Data will not be saved.");
    }
}

DailySummaryData DailySummaryManager::get_summary_for_date(time_t date) {
    std::string filepath = get_filepath_for_date(date);
    DailySummaryData summary{}; // FIX: Use C++ aggregate initialization to zero-initialize and suppress warnings.
    summary.date = get_start_of_day(date);

    char* buffer = nullptr;
    size_t size = 0;
    if (!littlefs_manager_read_file(filepath.c_str(), &buffer, &size) || !buffer) {
        return summary; // File doesn't exist, return empty summary
    }

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON from %s", filepath.c_str());
        return summary;
    }

    cJSON* item;
    item = cJSON_GetObjectItem(root, "journal_path");
    if (cJSON_IsString(item)) {
        summary.journal_entry_path = item->valuestring;
    }

    item = cJSON_GetObjectItem(root, "completed_habit_ids");
    if (cJSON_IsArray(item)) {
        cJSON* habit_id_json;
        cJSON_ArrayForEach(habit_id_json, item) {
            if (cJSON_IsNumber(habit_id_json)) {
                summary.completed_habit_ids.push_back(habit_id_json->valueint);
            }
        }
    }

    item = cJSON_GetObjectItem(root, "voice_note_paths");
    if (cJSON_IsArray(item)) {
        cJSON* note_path_json;
        cJSON_ArrayForEach(note_path_json, item) {
            if (cJSON_IsString(note_path_json)) {
                summary.voice_note_paths.push_back(note_path_json->valuestring);
            }
        }
    }

    cJSON_Delete(root);
    return summary;
}

time_t DailySummaryManager::get_latest_summary_date() {
    std::string dir_path = get_summary_dir_path();
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        return 0; // No directory, no summaries
    }

    struct dirent* entry;
    std::string latest_filename = "";
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            if (latest_filename == "" || strcmp(entry->d_name, latest_filename.c_str()) > 0) {
                latest_filename = entry->d_name;
            }
        }
    }
    closedir(dir);

    if (latest_filename.empty()) {
        return 0;
    }
    
    // Parse date from filename YYYYMMDD.json
    struct tm timeinfo = {};
    sscanf(latest_filename.c_str(), "%4d%2d%2d.json", &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday);
    timeinfo.tm_year -= 1900;
    timeinfo.tm_mon -= 1;
    
    return mktime(&timeinfo);
}

void DailySummaryManager::add_completed_habit(time_t date, uint32_t habit_id) {
    DailySummaryData summary = get_summary_for_date(date);
    auto& ids = summary.completed_habit_ids;
    if (std::find(ids.begin(), ids.end(), habit_id) == ids.end()) {
        ids.push_back(habit_id);
        save_summary(summary);
    }
}

void DailySummaryManager::remove_completed_habit(time_t date, uint32_t habit_id) {
    DailySummaryData summary = get_summary_for_date(date);
    auto& ids = summary.completed_habit_ids;
    auto it = std::remove(ids.begin(), ids.end(), habit_id);
    if (it != ids.end()) {
        ids.erase(it, ids.end());
        save_summary(summary);
    }
}

void DailySummaryManager::set_journal_path(time_t date, const std::string& path) {
    DailySummaryData summary = get_summary_for_date(date);
    summary.journal_entry_path = path;
    save_summary(summary);
}

void DailySummaryManager::add_voice_note_path(time_t date, const std::string& path) {
    DailySummaryData summary = get_summary_for_date(date);
    summary.voice_note_paths.push_back(path);
    save_summary(summary);
}