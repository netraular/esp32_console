#include "room_object_manager.h"
#include "controllers/littlefs_manager/littlefs_manager.h"
#include "models/asset_config.h"
#include "esp_log.h"
#include <cJSON.h>

static const char* TAG = "RoomObjectManager";

RoomObjectManager::RoomObjectManager() {
    // Construct the full path to the layout file once.
    char path_buf[128];
    snprintf(path_buf, sizeof(path_buf), "%s%s%s", USER_DATA_BASE_PATH, ROOM_SUBPATH, ROOM_LAYOUT_FILENAME);
    m_layout_file_path = path_buf;

    // Ensure the directory exists.
    char dir_buf[128];
    snprintf(dir_buf, sizeof(dir_buf), "%s%s", USER_DATA_BASE_PATH, ROOM_SUBPATH);
    littlefs_manager_ensure_dir_exists(dir_buf);

    load_layout();
}

void RoomObjectManager::load_layout() {
    m_placed_objects.clear();
    
    char* buffer = nullptr;
    size_t size = 0;
    if (!littlefs_manager_read_file(m_layout_file_path.c_str(), &buffer, &size) || buffer == nullptr) {
        ESP_LOGI(TAG, "Room layout file not found or empty. Starting with a blank room.");
        return;
    }

    cJSON* root = cJSON_Parse(buffer);
    free(buffer);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse room layout JSON. Error: %s", cJSON_GetErrorPtr());
        return;
    }

    if (cJSON_IsArray(root)) {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, root) {
            PlacedFurniture pf;
            pf.type_name = cJSON_GetObjectItem(item, "type")->valuestring;
            pf.grid_x = cJSON_GetObjectItem(item, "x")->valueint;
            pf.grid_y = cJSON_GetObjectItem(item, "y")->valueint;
            pf.grid_z = cJSON_GetObjectItem(item, "z")->valuedouble;
            pf.direction = cJSON_GetObjectItem(item, "dir")->valueint;
            m_placed_objects.push_back(pf);
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %zu objects into the room.", m_placed_objects.size());
}

void RoomObjectManager::save_layout() {
    cJSON* root = cJSON_CreateArray();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON array for saving layout.");
        return;
    }

    for (const auto& pf : m_placed_objects) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "type", pf.type_name.c_str());
        cJSON_AddNumberToObject(item, "x", pf.grid_x);
        cJSON_AddNumberToObject(item, "y", pf.grid_y);
        cJSON_AddNumberToObject(item, "z", pf.grid_z);
        cJSON_AddNumberToObject(item, "dir", pf.direction);
        cJSON_AddItemToArray(root, item);
    }

    char* json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_string) {
        if (!littlefs_manager_write_file(m_layout_file_path.c_str(), json_string)) {
            ESP_LOGE(TAG, "Failed to write room layout to file!");
        } else {
            ESP_LOGD(TAG, "Room layout saved successfully.");
        }
        free(json_string);
    }
}

const std::vector<PlacedFurniture>& RoomObjectManager::get_all_objects() const {
    return m_placed_objects;
}

const PlacedFurniture* RoomObjectManager::get_object_at(int grid_x, int grid_y) const {
    for (const auto& obj : m_placed_objects) {
        // This is a simple check; a real implementation would need to account for object dimensions.
        if (obj.grid_x == grid_x && obj.grid_y == grid_y) {
            return &obj;
        }
    }
    return nullptr;
}

bool RoomObjectManager::remove_object_at(int grid_x, int grid_y) {
    for (auto it = m_placed_objects.begin(); it != m_placed_objects.end(); ++it) {
        if (it->grid_x == grid_x && it->grid_y == grid_y) {
            m_placed_objects.erase(it);
            return true;
        }
    }
    return false;
}

bool RoomObjectManager::add_object(const PlacedFurniture& object) {
    if (get_object_at(object.grid_x, object.grid_y) != nullptr) {
        ESP_LOGW(TAG, "Cannot add object, position (%d, %d) is already occupied.", object.grid_x, object.grid_y);
        return false;
    }
    m_placed_objects.push_back(object);
    return true;
}