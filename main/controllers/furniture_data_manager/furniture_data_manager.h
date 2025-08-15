#ifndef FURNITURE_DATA_MANAGER_H
#define FURNITURE_DATA_MANAGER_H

#include "models/furniture_data_model.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

/**
 * @brief Manages loading and parsing all furniture definitions from the SD card.
 *
 * This singleton class scans a directory for furniture JSON files, parses them,
 * and provides a central, read-only repository of all available furniture types.
 */
class FurnitureDataManager {
public:
    FurnitureDataManager(const FurnitureDataManager&) = delete;
    FurnitureDataManager& operator=(const FurnitureDataManager&) = delete;

    /**
     * @brief Gets the singleton instance of the manager.
     */
    static FurnitureDataManager& get_instance();

    /**
     * @brief Initializes the manager and loads all furniture definitions.
     * Must be called after the SD card is mounted.
     */
    void init();

    /**
     * @brief Retrieves a constant pointer to a furniture definition by its type name.
     * @param type_name The unique name of the furniture type (e.g., "ads_gsArcade_2").
     * @return A const pointer to the FurnitureData, or nullptr if not found.
     */
    const FurnitureData* get_definition(const std::string& type_name) const;

private:
    FurnitureDataManager() = default;
    ~FurnitureDataManager() = default;

    void load_definitions_from_sd();
    bool parse_furniture_file(const std::string& full_path);

    // Callback for sd_manager_list_files
    static void file_iterator_callback(const char* name, bool is_dir, void* user_data);

    std::unordered_map<std::string, std::unique_ptr<FurnitureData>> m_definitions;
    bool m_initialized = false;
    mutable std::mutex m_mutex;
};

#endif // FURNITURE_DATA_MANAGER_H