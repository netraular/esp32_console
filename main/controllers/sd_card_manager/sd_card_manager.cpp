#include "sd_card_manager.h"
#include "config.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <dirent.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <errno.h> 
#include <stdio.h>  

static const char* TAG = "SD_MGR";

static bool s_bus_initialized = false;
static bool s_is_mounted = false;
static sdmmc_card_t* s_card;
static const char* MOUNT_POINT = "/sdcard";

// --- Funciones existentes ---
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
    ESP_LOGI(TAG, "Attempting to mount SD card...");
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_HOST;
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = SD_HOST;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 10;
    mount_config.allocation_unit_size = 16 * 1024;

    esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. It may be missing a FAT format.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize card (%s).", esp_err_to_name(ret));
        }
        s_is_mounted = false;
        return false;
    }
    ESP_LOGI(TAG, "SD Card mounted successfully at %s", MOUNT_POINT);
    sdmmc_card_print_info(stdout, s_card);
    s_is_mounted = true;
    return true;
}

void sd_manager_unmount(void) {
    if (s_is_mounted) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
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

bool sd_manager_check_ready(void) {
    if (!s_is_mounted) {
        if (!sd_manager_mount()) {
            return false;
        }
    }

    DIR* dir = opendir(MOUNT_POINT);
    if (dir) {
        closedir(dir);
        return true;
    }

    ESP_LOGW(TAG, "Check ready failed: opendir could not read the root directory. Card may be disconnected.");
    sd_manager_unmount();
    return false;
}

const char* sd_manager_get_mount_point(void) {
    return MOUNT_POINT;
}

bool sd_manager_list_files(const char* path, file_iterator_cb_t cb, void* user_data) {
    if (!s_is_mounted || !cb) {
        return false;
    }
    DIR* dir = opendir(path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return false;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // --- VUELVE A AÑADIR ESTE LOG ---
        ESP_LOGI(TAG, "FATFS readdir() returned: '%s'", entry->d_name);
        // --- FIN DEL LOG ---
        bool is_dir = (entry->d_type == DT_DIR);
        cb(entry->d_name, is_dir, user_data);
    }
    closedir(dir);
    return true;
}

bool sd_manager_delete_item(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        ESP_LOGE(TAG, "Cannot stat path: %s. Error: %s", path, strerror(errno));
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
    if (mkdir(path, 0777) == 0) {
        ESP_LOGI(TAG, "Created directory: %s", path);
        return true;
    }
    ESP_LOGE(TAG, "Failed to create directory: %s. Error: %s", path, strerror(errno));
    return false;
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
        ESP_LOGE(TAG, "Failed to open file for reading: %s. Error: %s", path, strerror(errno));
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
    ESP_LOGI(TAG, "Read %zu bytes from %s", *size, path);
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