#include "sd_card_manager.h"
#include "config/board_config.h"
#include "config/app_config.h"
#include "models/asset_config.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <dirent.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <errno.h> 
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "SD_MGR";
#define MOUNT_ATTEMPT_COUNT 3
#define MOUNT_RETRY_DELAY_MS 200

static bool s_bus_initialized = false;
static bool s_is_mounted = false;
static sdmmc_card_t* s_card;

// (El resto del archivo, desde sd_manager_init hasta sd_manager_get_mount_point, no cambia)
// ...
bool sd_manager_init(void) {
    if (s_bus_initialized) {
        ESP_LOGW(TAG, "SPI bus already initialized.");
        return true;
    }
    ESP_LOGI(TAG, "Initializing SPI bus for SD card (SPI3_HOST)");
    
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = SD_SPI_MOSI_PIN;
    bus_cfg.miso_io_num = SD_SPI_MISO_PIN;
    bus_cfg.sclk_io_num = SD_SPI_SCLK_PIN;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4096;

    esp_err_t ret = spi_bus_initialize(SD_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus. Error: %s", esp_err_to_name(ret));
        return false;
    }
    s_bus_initialized = true;
    return true;
}

bool sd_manager_mount(void) {
    if (s_is_mounted) {
        return true;
    }
    if (!s_bus_initialized) {
        ESP_LOGE(TAG, "SPI bus has not been initialized. Call sd_manager_init() first.");
        return false;
    }

    ESP_LOGD(TAG, "Waiting for SD card power-on stabilization...");
    vTaskDelay(pdMS_TO_TICKS(100));

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = SD_HOST;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 10;
    mount_config.allocation_unit_size = 16 * 1024;

    esp_err_t ret = ESP_FAIL;
    for (int i = 1; i <= MOUNT_ATTEMPT_COUNT; ++i) {
        ESP_LOGI(TAG, "Attempting to mount SD card (attempt %d/%d)...", i, MOUNT_ATTEMPT_COUNT);
        ret = esp_vfs_fat_sdspi_mount(SD_CARD_ROOT_PATH, &host, &slot_config, &mount_config, &s_card);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "SD Card mounted successfully at %s", SD_CARD_ROOT_PATH);
            sdmmc_card_print_info(stdout, s_card);
            s_is_mounted = true;
            return true;
        }

        ESP_LOGW(TAG, "Mount attempt %d failed (%s).", i, esp_err_to_name(ret));
        
        esp_vfs_fat_sdcard_unmount(SD_CARD_ROOT_PATH, s_card);
        s_card = NULL;

        if (i < MOUNT_ATTEMPT_COUNT) {
            vTaskDelay(pdMS_TO_TICKS(MOUNT_RETRY_DELAY_MS));
        }
    }

    ESP_LOGE(TAG, "Failed to mount SD card after %d attempts.", MOUNT_ATTEMPT_COUNT);
    s_is_mounted = false;
    return false;
}

void sd_manager_unmount(void) {
    if (s_is_mounted) {
        esp_vfs_fat_sdcard_unmount(SD_CARD_ROOT_PATH, s_card);
        s_is_mounted = false;
        s_card = NULL;
        ESP_LOGI(TAG, "SD Card unmounted.");
    }
}

void sd_manager_deinit(void) {
    sd_manager_unmount();
    if (s_bus_initialized) {
        spi_bus_free(SD_HOST);
        s_bus_initialized = false;
        ESP_LOGI(TAG, "SPI bus for SD released.");
    }
}

bool sd_manager_is_mounted(void) {
    return s_is_mounted;
}

bool sd_manager_file_exists(const char* path) {
    if (!s_is_mounted) {
        ESP_LOGW(TAG, "Cannot check file existence, SD card not mounted.");
        return false;
    }
    struct stat st;
    if (stat(path, &st) == 0) {
        // It exists, check if it's a file, not a directory.
        return S_ISREG(st.st_mode);
    }
    // `stat` returns -1 if file not found, `errno` is set to ENOENT.
    if (errno == ENOENT) {
        return false; // File does not exist, this is not an error.
    }
    // Another error occurred.
    ESP_LOGE(TAG, "Error stating file %s: %s", path, strerror(errno));
    return false;
}

bool sd_manager_check_ready(void) {
    if (!s_is_mounted) {
        if (!sd_manager_mount()) {
            return false;
        }
    }

    DIR* dir = opendir(SD_CARD_ROOT_PATH);
    if (dir) {
        closedir(dir);
        return true;
    }

    ESP_LOGW(TAG, "Check ready failed: opendir could not access root directory. Card may be disconnected.");
    sd_manager_unmount();
    return false;
}

const char* sd_manager_get_mount_point(void) {
    return SD_CARD_ROOT_PATH;
}

bool sd_manager_list_files(const char* path, file_iterator_cb_t cb, void* user_data) {
    if (!s_is_mounted || !cb) {
        return false;
    }
    DIR* dir = opendir(path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s. Error: %s", path, strerror(errno));
        return false;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        bool is_dir = (entry->d_type == DT_DIR);
        cb(entry->d_name, is_dir, user_data);
    }
    closedir(dir);
    return true;
}

bool sd_manager_delete_item(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        ESP_LOGW(TAG, "Cannot stat path for deletion: %s. Error: %s", path, strerror(errno));
        return false;
    }

    if (S_ISDIR(st.st_mode)) {
        if (rmdir(path) == 0) {
            ESP_LOGI(TAG, "Removed directory: %s", path);
            return true;
        }
        ESP_LOGE(TAG, "Failed to remove directory: %s. Error: %s", path, strerror(errno));
        return false;
    } else {
        if (unlink(path) == 0) {
            ESP_LOGI(TAG, "Removed file: %s", path);
            return true;
        }
        ESP_LOGE(TAG, "Failed to remove file: %s. Error: %s", path, strerror(errno));
        return false;
    }
}

bool sd_manager_rename_item(const char* old_path, const char* new_path) {
    if (rename(old_path, new_path) == 0) {
        ESP_LOGI(TAG, "Renamed/Moved '%s' to '%s'", old_path, new_path);
        return true;
    }
    ESP_LOGE(TAG, "Failed to rename/move '%s' to '%s'. Error: %s", old_path, new_path, strerror(errno));
    return false;
}

bool sd_manager_create_directory(const char* path) {
    char tmp_path[256];
    char* p = NULL;
    size_t len;

    snprintf(tmp_path, sizeof(tmp_path), "%s", path);
    len = strlen(tmp_path);
    if (tmp_path[len - 1] == '/') {
        tmp_path[len - 1] = 0;
    }

    for (p = tmp_path + 1; *p; p++) {
        if (*p == '/') {
            *p = 0; // Temporarily end the string
            if (mkdir(tmp_path, 0777) != 0) {
                if (errno != EEXIST) {
                    ESP_LOGE(TAG, "Failed to create directory %s. Error: %s", tmp_path, strerror(errno));
                    *p = '/'; // Restore slash before returning
                    return false;
                }
            }
            *p = '/'; // Restore slash
        }
    }

    if (mkdir(tmp_path, 0777) != 0) {
        if (errno != EEXIST) {
            ESP_LOGE(TAG, "Failed to create final directory %s. Error: %s", tmp_path, strerror(errno));
            return false;
        }
    }
    
    ESP_LOGI(TAG, "Ensured directory path exists: %s", path);
    return true;
}

bool sd_manager_create_file(const char* path) {
    FILE* f = fopen(path, "w");
    if (f) {
        fclose(f);
        ESP_LOGI(TAG, "Created file: %s", path);
        return true;
    }
    ESP_LOGE(TAG, "Failed to create file: %s. Error: %s", path, strerror(errno));
    return false;
}

bool sd_manager_read_file(const char* path, char** buffer, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        if (errno != ENOENT) {
            ESP_LOGE(TAG, "Failed to open file for reading: %s. Error: %s", path, strerror(errno));
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
    ESP_LOGD(TAG, "Read %zu bytes from %s", *size, path);
    return true;
}

bool sd_manager_write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s. Error: %s", path, strerror(errno));
        return false;
    }
    
    size_t content_len = strlen(content);
    if (fwrite(content, 1, content_len, f) != content_len) {
        ESP_LOGE(TAG, "Failed to write full content to file: %s", path);
        fclose(f);
        return false;
    }

    fclose(f);
    ESP_LOGI(TAG, "Wrote %zu bytes to %s", content_len, path);
    return true;
}