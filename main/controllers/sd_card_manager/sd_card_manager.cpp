#include "sd_card_manager.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "config.h"
#include <dirent.h>
#include <string.h>
#include <stdbool.h>

static const char *TAG = "SD_MGR";
static const char *MOUNT_POINT = "/sdcard";
static sdmmc_card_t *card;
static bool is_mounted = false;
static spi_host_device_t spi_host;

bool sd_manager_init(void) {
    if (is_mounted) {
        ESP_LOGI(TAG, "SD card already initialized and mounted.");
        return true;
    }

    esp_err_t ret;

    // --- [FIX] Inicializar toda la estructura a cero para eliminar warnings ---
    spi_bus_config_t bus_cfg = {}; 
    bus_cfg.mosi_io_num = SD_SPI_MOSI_PIN;
    bus_cfg.miso_io_num = SD_SPI_MISO_PIN;
    bus_cfg.sclk_io_num = SD_SPI_SCLK_PIN;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4092;
    
    spi_host = SD_HOST;

    ret = spi_bus_initialize(spi_host, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize spi bus.");
        return false;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = spi_host;
    host.max_freq_khz = 20000;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = spi_host;

    // --- [FIX] Inicializar toda la estructura a cero para eliminar warnings ---
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 16 * 1024;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors.", esp_err_to_name(ret));
        }
        spi_bus_free(spi_host);
        return false;
    }

    ESP_LOGI(TAG, "SD Card mounted at %s", MOUNT_POINT);
    sdmmc_card_print_info(stdout, card);
    is_mounted = true;
    return true;
}

void sd_manager_deinit(void) {
    if (is_mounted) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        ESP_LOGI(TAG, "SD Card unmounted.");
        
        spi_bus_free(spi_host);
        ESP_LOGI(TAG, "SPI bus freed.");
        
        is_mounted = false;
    }
}

bool sd_manager_is_mounted(void) {
    return is_mounted;
}

const char *sd_manager_get_mount_point(void) {
    return MOUNT_POINT;
}

void sd_manager_list_files(const char *path, file_iterator_cb_t callback, void *user_data) {
    if (!is_mounted || !callback) {
        return;
    }

    ESP_LOGI(TAG, "Listing directory: %s", path);
    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue; // Ignorar archivos ocultos y . ..
        }
        bool is_dir = (entry->d_type == DT_DIR);
        callback(entry->d_name, is_dir, user_data);
    }
    closedir(dir);
}