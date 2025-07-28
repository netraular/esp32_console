#include "littlefs_manager.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h> // Include for unlink()

static const char *TAG = "LFS_MGR";
static char s_mount_point[32] = {0};
static const char* s_partition_label = NULL;

bool littlefs_manager_init(const char* partition_label) {
    if (s_mount_point[0] != '\0') {
        ESP_LOGW(TAG, "LittleFS already initialized.");
        return true;
    }

    snprintf(s_mount_point, sizeof(s_mount_point), "/%s", partition_label);
    s_partition_label = partition_label;

    // Use two-step initialization for robust, warning-free code.
    // 1. Zero-initialize all members of the struct.
    esp_vfs_littlefs_conf_t conf = {}; 
    // 2. Set the specific members needed.
    conf.base_path = s_mount_point;
    conf.partition_label = s_partition_label;
    conf.format_if_mount_failed = true;
    conf.dont_mount = false;

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        s_mount_point[0] = '\0';
        return false;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(s_partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    ESP_LOGI(TAG, "LittleFS mounted on %s", s_mount_point);
    return true;
}

void littlefs_manager_deinit(void) {
    if (s_mount_point[0] != '\0') {
        esp_vfs_littlefs_unregister(s_partition_label);
        s_mount_point[0] = '\0';
        s_partition_label = NULL;
        ESP_LOGI(TAG, "LittleFS unmounted");
    }
}

const char* littlefs_manager_get_mount_point(void) {
    return s_mount_point;
}

static bool build_full_path(const char* relative_path, char* full_path_buf, size_t buf_size) {
    if (snprintf(full_path_buf, buf_size, "%s/%s", s_mount_point, relative_path) >= buf_size) {
        ESP_LOGE(TAG, "Full path exceeds buffer size");
        return false;
    }
    return true;
}

bool littlefs_manager_ensure_dir_exists(const char* relative_path) {
    char full_path[128];
    if (!build_full_path(relative_path, full_path, sizeof(full_path))) return false;

    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    if (mkdir(full_path, 0755) == 0) {
        ESP_LOGI(TAG, "Created directory: %s", full_path);
        return true;
    }
    ESP_LOGE(TAG, "Failed to create directory: %s", full_path);
    return false;
}

bool littlefs_manager_file_exists(const char* filename) {
    char full_path[128];
    if (!build_full_path(filename, full_path, sizeof(full_path))) return false;
    struct stat st;
    return (stat(full_path, &st) == 0);
}

bool littlefs_manager_read_file(const char* filename, char** buffer, size_t* size) {
    char full_path[128];
    if (!build_full_path(filename, full_path, sizeof(full_path))) return false;
    
    FILE* f = fopen(full_path, "rb");
    if (f == NULL) {
        ESP_LOGD(TAG, "File not found: %s", full_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0) {
         ESP_LOGE(TAG, "Failed to get file size for %s", full_path);
         fclose(f);
         return false;
    }

    *buffer = (char*)malloc(file_size + 1);
    if (*buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for reading file");
        fclose(f);
        return false;
    }

    size_t bytes_read = fread(*buffer, 1, file_size, f);
    fclose(f);

    if (bytes_read != file_size) {
        ESP_LOGE(TAG, "Failed to read entire file. Read %d of %ld bytes", bytes_read, file_size);
        free(*buffer);
        *buffer = NULL;
        return false;
    }

    (*buffer)[file_size] = '\0';
    *size = file_size;
    return true;
}

bool littlefs_manager_write_file(const char* filename, const char* content) {
    char full_path[128];
    if (!build_full_path(filename, full_path, sizeof(full_path))) return false;
    
    FILE* f = fopen(full_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path);
        return false;
    }
    fprintf(f, "%s", content);
    fclose(f);
    return true;
}

bool littlefs_manager_delete_file(const char* filename) {
    char full_path[128];
    if (!build_full_path(filename, full_path, sizeof(full_path))) return false;
    
    if (unlink(full_path) == 0) {
        ESP_LOGD(TAG, "Deleted file: %s", full_path);
        return true;
    }
    ESP_LOGE(TAG, "Failed to delete file: %s", full_path);
    return false;
}

bool littlefs_manager_rename_file(const char* old_name, const char* new_name) {
    char old_full_path[128];
    if (!build_full_path(old_name, old_full_path, sizeof(old_full_path))) return false;
    
    char new_full_path[128];
    if (!build_full_path(new_name, new_full_path, sizeof(new_full_path))) return false;

    if (rename(old_full_path, new_full_path) == 0) {
        ESP_LOGD(TAG, "Renamed %s to %s", old_full_path, new_full_path);
        return true;
    }
    ESP_LOGE(TAG, "Failed to rename %s to %s", old_full_path, new_full_path);
    return false;
}