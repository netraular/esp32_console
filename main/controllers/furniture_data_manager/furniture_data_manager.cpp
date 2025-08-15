#include "furniture_data_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "models/asset_config.h"
#include "esp_log.h"
#include <cJSON.h>
#include <string>
#include <cstring> // For strrchr and strcmp

static const char* TAG = "FURNI_DATA_MGR";

FurnitureDataManager& FurnitureDataManager::get_instance() {
    static FurnitureDataManager instance;
    return instance;
}

void FurnitureDataManager::init() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) {
        ESP_LOGW(TAG, "Already initialized.");
        return;
    }
    ESP_LOGI(TAG, "Initializing...");
    load_definitions_from_sd();
    m_initialized = true;
    ESP_LOGI(TAG, "Loaded %zu furniture definitions.", m_definitions.size());
}

const FurnitureData* FurnitureDataManager::get_definition(const std::string& type_name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_definitions.find(type_name);
    if (it != m_definitions.end()) {
        return it->second.get();
    }
    return nullptr;
}

void FurnitureDataManager::load_definitions_from_sd() {
    if (!sd_manager_is_mounted()) {
        ESP_LOGE(TAG, "Cannot load definitions, SD card is not mounted.");
        return;
    }

    char furniture_dir_path[256];
    snprintf(furniture_dir_path, sizeof(furniture_dir_path), "%s%s%s",
             sd_manager_get_mount_point(), ASSETS_BASE_SUBPATH, ASSETS_FURNITURE_SUBPATH);

    ESP_LOGI(TAG, "Scanning for furniture definitions in %s", furniture_dir_path);
    sd_manager_list_files(furniture_dir_path, file_iterator_callback, this);
}

void FurnitureDataManager::file_iterator_callback(const char* name, bool is_dir, void* user_data) {
    // We are interested in directories, not files in the root furniture folder.
    if (!is_dir) return;

    // Ignore '.' and '..' directories
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return;

    FurnitureDataManager* manager = static_cast<FurnitureDataManager*>(user_data);

    // --- MODIFICATION START ---
    // Construct the path to the expected "furni.json" file inside the directory.
    char json_path[256];
    snprintf(json_path, sizeof(json_path), "%s%s%s%s/furni.json",
             sd_manager_get_mount_point(), ASSETS_BASE_SUBPATH, ASSETS_FURNITURE_SUBPATH, name);
    // --- MODIFICATION END ---

    ESP_LOGD(TAG, "Found furniture directory '%s', checking for definition at '%s'", name, json_path);

    if (sd_manager_file_exists(json_path)) {
        manager->parse_furniture_file(json_path);
    } else {
        ESP_LOGW(TAG, "Directory '%s' does not contain a 'furni.json' definition file. Skipping.", name);
    }
}

bool FurnitureDataManager::parse_furniture_file(const std::string& full_path) {
    char* buffer = nullptr;
    size_t size = 0;
    if (!sd_manager_read_file(full_path.c_str(), &buffer, &size) || buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to read furniture file: %s", full_path.c_str());
        return false;
    }

    cJSON* root = cJSON_Parse(buffer);
    free(buffer);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON from %s. Error: %s", full_path.c_str(), cJSON_GetErrorPtr());
        return false;
    }

    auto furni_data = std::make_unique<FurnitureData>();
    cJSON* type_item = cJSON_GetObjectItem(root, "type");
    if (cJSON_IsString(type_item)) {
        furni_data->type_name = type_item->valuestring;
    } else {
        ESP_LOGE(TAG, "JSON file '%s' is missing 'type' field.", full_path.c_str());
        cJSON_Delete(root);
        return false;
    }
    
    // It's crucial that the type name from the JSON matches the hardcoded name in RoomView.
    // Let's verify that the file you're loading actually defines "ads_gsArcade_2".
    if (furni_data->type_name != "ads_gsArcade_2" && full_path.find("ads_gsArcade_2") != std::string::npos) {
        ESP_LOGW(TAG, "Mismatch: File in folder 'ads_gsArcade_2' defines type '%s'", furni_data->type_name.c_str());
    }


    cJSON* logic = cJSON_GetObjectItem(root, "logic");
    cJSON* dimensions = cJSON_GetObjectItem(logic, "dimensions");
    furni_data->dimensions.x = atoi(cJSON_GetObjectItem(dimensions, "x")->valuestring);
    furni_data->dimensions.y = atoi(cJSON_GetObjectItem(dimensions, "y")->valuestring);
    furni_data->dimensions.z = atof(cJSON_GetObjectItem(dimensions, "z")->valuestring);

    cJSON* viz = cJSON_GetObjectItem(root, "visualization");
    cJSON* viz64 = cJSON_GetObjectItem(viz, "64");
    if (cJSON_IsObject(viz64)) {
        furni_data->layer_count = atoi(cJSON_GetObjectItem(viz64, "layerCount")->valuestring);
    }
    
    // For simplicity, we are not parsing all details like assets, directions etc.
    // This is a stub for future expansion to render actual sprites.

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Successfully parsed furniture type: %s from %s", furni_data->type_name.c_str(), full_path.c_str());
    m_definitions[furni_data->type_name] = std::move(furni_data);
    return true;
}