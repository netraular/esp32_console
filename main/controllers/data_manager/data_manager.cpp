#include "data_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char* TAG = "DATA_MGR";
static const char* NVS_NAMESPACE = "storage"; // Namespace para organizar los datos en NVS

static bool s_is_initialized = false;

void data_manager_init(void) {
    if (s_is_initialized) {
        ESP_LOGW(TAG, "Data manager already initialized.");
        return;
    }
    // Asumimos que nvs_flash_init() ya fue llamado en main.cpp
    s_is_initialized = true;
    ESP_LOGI(TAG, "Data Manager initialized.");
}

bool data_manager_set_u32(const char* key, uint32_t value) {
    if (!s_is_initialized) {
        ESP_LOGE(TAG, "Manager not initialized.");
        return false;
    }

    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_u32(my_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) writing key '%s' to NVS!", esp_err_to_name(err), key);
        nvs_close(my_handle);
        return false;
    }

    // Commit written value.
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing NVS changes!", esp_err_to_name(err));
        nvs_close(my_handle);
        return false;
    }

    nvs_close(my_handle);
    ESP_LOGD(TAG, "Successfully set u32 key '%s' = %lu", key, value);
    return true;
}

bool data_manager_get_u32(const char* key, uint32_t* value) {
    if (!s_is_initialized || !value) {
        ESP_LOGE(TAG, "Manager not initialized or value pointer is null.");
        return false;
    }

    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return false;
    }

    err = nvs_get_u32(my_handle, key, value);
    nvs_close(my_handle);

    switch (err) {
        case ESP_OK:
            ESP_LOGD(TAG, "Successfully got u32 key '%s' = %lu", key, *value);
            return true;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGI(TAG, "The key '%s' is not initialized yet in NVS.", key);
            return false;
        default:
            ESP_LOGE(TAG, "Error (%s) reading key '%s' from NVS!", esp_err_to_name(err), key);
            return false;
    }
}