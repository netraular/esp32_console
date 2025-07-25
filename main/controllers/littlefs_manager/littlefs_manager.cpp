#include "littlefs_manager.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include <string>
#include <sys/stat.h>
#include <cstring>
#include <errno.h>

static const char* TAG = "LFS_MGR";
static const char* MOUNT_POINT = "/fs";
static bool s_is_mounted = false;

// --- Helper to build full VFS paths ---
static std::string build_full_path(const char* relative_path) {
    return std::string(MOUNT_POINT) + "/" + relative_path;
}

bool littlefs_manager_init(const char* partition_label) {
    if (s_is_mounted) {
        ESP_LOGI(TAG, "LittleFS already mounted.");
        return true;
    }

    ESP_LOGI(TAG, "Initializing LittleFS on partition '%s'", partition_label);

    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = MOUNT_POINT;
    conf.partition_label = partition_label;
    conf.format_if_mount_failed = true;

    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition '%s'", partition_label);
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return false;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    
    s_is_mounted = true;
    ESP_LOGI(TAG, "LittleFS mounted on %s", MOUNT_POINT);
    return true;
}

void littlefs_manager_deinit(void) {
    if (s_is_mounted) {
        esp_vfs_littlefs_unregister(MOUNT_POINT);
        s_is_mounted = false;
        ESP_LOGI(TAG, "LittleFS unmounted.");
    }
}

const char* littlefs_manager_get_mount_point(void) {
    return MOUNT_POINT;
}

bool littlefs_manager_ensure_dir_exists(const char* relative_path) {
    std::string full_path_str = build_full_path(relative_path);
    const char* full_path = full_path_str.c_str();

    struct stat st;
    if (stat(full_path, &st) == 0) {
        // Path exists, check if it's a directory
        if (S_ISDIR(st.st_mode)) {
            ESP_LOGD(TAG, "Directory already exists: %s", full_path);
            return true;
        } else {
            ESP_LOGE(TAG, "Path exists but is not a directory: %s", full_path);
            return false;
        }
    } else {
        // Path does not exist, try to create it
        if (errno == ENOENT) {
            if (mkdir(full_path, 0755) == 0) {
                ESP_LOGI(TAG, "Created directory: %s", full_path);
                return true;
            } else {
                ESP_LOGE(TAG, "Failed to create directory %s: %s", full_path, strerror(errno));
                return false;
            }
        } else {
            ESP_LOGE(TAG, "Failed to stat directory %s: %s", full_path, strerror(errno));
            return false;
        }
    }
}

bool littlefs_manager_file_exists(const char* filename) {
    struct stat st;
    std::string full_path = build_full_path(filename);
    return (stat(full_path.c_str(), &st) == 0);
}

bool littlefs_manager_read_file(const char* filename, char** buffer, size_t* size) {
    if (!s_is_mounted) return false;

    std::string full_path = build_full_path(filename);
    FILE* f = fopen(full_path.c_str(), "rb");
    if (!f) {
        // Don't log an error if the file simply doesn't exist
        if(errno != ENOENT) {
            ESP_LOGE(TAG, "Failed to open file for reading: %s. Error: %s", full_path.c_str(), strerror(errno));
        }
        return false;
    }

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    *buffer = (char*)malloc(*size + 1);
    if (!*buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for file content");
        fclose(f);
        return false;
    }

    if (fread(*buffer, 1, *size, f) != *size) {
        ESP_LOGE(TAG, "Failed to read full file content");
        free(*buffer);
        *buffer = NULL;
        fclose(f);
        return false;
    }

    (*buffer)[*size] = '\0';
    fclose(f);
    ESP_LOGD(TAG, "Read %zu bytes from %s", *size, full_path.c_str());
    return true;
}

bool littlefs_manager_write_file(const char* filename, const char* content) {
    if (!s_is_mounted) return false;

    std::string full_path = build_full_path(filename);
    FILE* f = fopen(full_path.c_str(), "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s. Error: %s", full_path.c_str(), strerror(errno));
        return false;
    }
    
    size_t content_len = strlen(content);
    if (fwrite(content, 1, content_len, f) != content_len) {
        ESP_LOGE(TAG, "Failed to write full content to file: %s", full_path.c_str());
        fclose(f);
        return false;
    }

    fclose(f);
    ESP_LOGI(TAG, "Wrote %zu bytes to %s", content_len, full_path.c_str());
    return true;
}