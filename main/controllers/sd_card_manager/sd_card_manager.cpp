#include "sd_card_manager.h"
#include "config.h"

#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

static const char *TAG = "SD_MGR";

#define MOUNT_POINT "/sdcard"

static bool is_mounted = false;
static sdmmc_card_t *card = NULL;

bool sd_manager_init(void) {
    if (is_mounted) {
        ESP_LOGI(TAG, "SD card already initialized.");
        return true;
    }

    ESP_LOGI(TAG, "Initializing SD card...");

    // --- CORRECCIÓN DEFINITIVA PARA ELIMINAR WARNINGS ---
    // 1. Inicializar toda la estructura a ceros.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    
    // 2. Asignar los valores específicos que necesitamos.
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 16 * 1024;
    // --- FIN DE LA CORRECCIÓN ---


    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = LCD_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = LCD_HOST; 

    esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. Check if SD card is formatted as FAT32.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Check connections (CS, MISO) and card format.", esp_err_to_name(ret));
        }
        is_mounted = false;
        return false;
    }

    ESP_LOGI(TAG, "SD Card mounted successfully at %s", MOUNT_POINT);
    sdmmc_card_print_info(stdout, card);
    is_mounted = true;
    return true;
}

// ... (El resto del archivo no cambia) ...

void sd_manager_deinit(void) {
    if (is_mounted && card != NULL) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        ESP_LOGI(TAG, "SD card unmounted.");
        is_mounted = false;
        card = NULL;
    }
}

bool sd_manager_is_mounted(void) {
    return is_mounted;
}

const char* sd_manager_get_mount_point(void) {
    return MOUNT_POINT;
}

void sd_manager_list_files(const char *path) {
    if (!is_mounted) {
        ESP_LOGE(TAG, "Cannot list files, SD card is not mounted.");
        return;
    }

    ESP_LOGI(TAG, "Listing directory: %s", path);
    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory '%s': %s", path, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[256 + 20];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat entry_stat;
        if (stat(full_path, &entry_stat) == -1) {
            ESP_LOGW(TAG, "Could not stat '%s', printing as is.", entry->d_name);
             printf("  - %s\n", entry->d_name);
            continue;
        }

        if (entry->d_type == DT_DIR) {
            printf("  DIR : %s/\n", entry->d_name);
        } else if (entry->d_type == DT_REG) {
            printf("  FILE: %s (size: %jd bytes)\n", entry->d_name, (intmax_t)entry_stat.st_size);
        } else {
            printf("  OTHER: %s\n", entry->d_name);
        }
    }
    closedir(dir);
}