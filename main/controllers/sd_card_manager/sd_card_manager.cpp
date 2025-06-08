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
#include "driver/spi_master.h"

static const char *TAG = "SD_MGR";

#define MOUNT_POINT "/sdcard"

static bool is_mounted = false;
static sdmmc_card_t *card = NULL;

bool sd_manager_init(void) {
    if (is_mounted) {
        ESP_LOGI(TAG, "SD card already initialized.");
        return true;
    }

    ESP_LOGI(TAG, "Initializing SD card on dedicated SPI bus (Host ID: %d)", SD_HOST);

    // --- CORRECCIÓN PARA WARNINGS ---
    // 1. Inicializar toda la estructura a ceros.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    // 2. Asignar los valores específicos que necesitamos.
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 16 * 1024;
    // --- FIN DE LA CORRECCIÓN ---

    // --- CORRECCIÓN PARA WARNINGS ---
    // 1. Inicializar toda la estructura a ceros.
    spi_bus_config_t bus_cfg = {};
    // 2. Asignar los valores específicos que necesitamos.
    bus_cfg.mosi_io_num = SD_SPI_MOSI_PIN;
    bus_cfg.miso_io_num = SD_SPI_MISO_PIN;
    bus_cfg.sclk_io_num = SD_SPI_SCLK_PIN;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4096;
    // --- FIN DE LA CORRECCIÓN ---

    esp_err_t ret = spi_bus_initialize(SD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus for SD Card (%s)", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "Dedicated SPI bus for SD Card initialized successfully.");

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = SD_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. Check if SD card is formatted as FAT32.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Check connections and card format.", esp_err_to_name(ret));
        }
        spi_bus_free(SD_HOST);
        is_mounted = false;
        return false;
    }

    ESP_LOGI(TAG, "SD Card mounted successfully at %s", MOUNT_POINT);
    sdmmc_card_print_info(stdout, card);
    is_mounted = true;
    return true;
}

void sd_manager_deinit(void) {
    if (is_mounted && card != NULL) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        spi_bus_free(SD_HOST); 
        ESP_LOGI(TAG, "SD card unmounted and dedicated SPI bus freed.");
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

void sd_manager_list_files(const char *path, file_iterator_cb_t callback, void *user_data) {
    if (!is_mounted) {
        ESP_LOGE(TAG, "Cannot list files, SD card is not mounted.");
        return;
    }
    if (!callback) {
        ESP_LOGE(TAG, "Callback function is NULL.");
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
        callback(entry->d_name, (entry->d_type == DT_DIR), user_data);
    }
    closedir(dir);
}