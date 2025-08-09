#include "pet_manager.h"
#include "controllers/data_manager/data_manager.h"
#include "models/asset_config.h" 
#include "esp_log.h"
#include "esp_random.h"
#include <algorithm>
#include <cstdio>
#include <vector>

// Set to 1 to enable a 7-minute pet lifecycle for rapid testing.
//Leave it as 1 since we are still in developing fase.
#define PET_LIFECYCLE_DEBUG_7_MINUTES 1

static const char* TAG = "PET_MGR";

// --- NVS Keys for Pet State Persistence ---
static const char* PET_BASE_ID_KEY = "pet_base_id";
static const char* PET_CURRENT_ID_KEY = "pet_curr_id";
static const char* PET_STAGE_POINTS_KEY = "pet_st_pts"; // Changed key for clarity
static const char* PET_NAME_KEY = "pet_name";
static const char* PET_START_TS_KEY = "pet_start_ts";
static const char* PET_END_TS_KEY = "pet_end_ts";
static const char* PET_COLL_PREFIX_KEY = "pet_coll_";

// --- Game Logic Constants ---
constexpr time_t SECONDS_IN_DAY = 86400;
constexpr int MIN_CYCLE_DURATION_DAYS = 10;
constexpr time_t EGG_HATCH_DURATION_SECONDS = 3 * 60; // 3 minutes for an egg to hatch

// --- Evolution Stage Time Percentages ---
constexpr float STAGE_2_EVOLUTION_PERCENT = 0.33f; // 1/3 of the way through
constexpr float STAGE_3_EVOLUTION_PERCENT = 0.66f; // 2/3 of the way through

PetManager& PetManager::get_instance() {
    static PetManager instance;
    return instance;
}

void PetManager::init() {
    if (s_is_initialized) return;
    ESP_LOGI(TAG, "Initializing Pet Manager...");
    load_state();
    s_is_initialized = true;
    ESP_LOGI(TAG, "Pet Manager initialized.");
}

const PetData* PetManager::get_pet_data(PetId id) const {
    auto it = PET_DATA_REGISTRY.find(id);
    if (it != PET_DATA_REGISTRY.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string PetManager::get_pet_name(PetId id) const {
    if (const auto* data = get_pet_data(id)) {
        return data->name;
    }
    return "Unknown";
}

std::string PetManager::get_pet_display_name(const PetState& state) const {
    if (state.current_pet_id == PetId::NONE) {
        return "Mysterious Egg";
    }
    if (!state.custom_name.empty()) {
        return state.custom_name;
    }
    return get_pet_name(state.current_pet_id);
}

PetId PetManager::select_random_hatchable_pet() {
    std::vector<PetId> available_pets;
    for (const auto& entry : s_collection) {
        if (!entry.collected) {
            if (const auto* data = get_pet_data(entry.base_id)) {
                if (data->can_hatch) {
                    available_pets.push_back(entry.base_id);
                }
            }
        }
    }

    if (available_pets.empty()) {
        ESP_LOGI(TAG, "All pets collected! Resetting collection for replay.");
        for (auto& entry : s_collection) entry.collected = false;
        save_collection();
        for (const auto& pair : PET_DATA_REGISTRY) {
            if (pair.second.can_hatch) {
                available_pets.push_back(pair.first);
            }
        }
    }
    
    if (available_pets.empty()) {
        ESP_LOGE(TAG, "No hatchable pets found in PET_DATA_REGISTRY!");
        return PetId::NONE;
    }
    
    uint32_t index = esp_random() % available_pets.size();
    PetId new_id = available_pets[index];

    ESP_LOGI(TAG, "Selected new pet: %s", get_pet_name(new_id).c_str());
    return new_id;
}

std::string PetManager::get_sprite_path_for_id(PetId id) const {
    char id_str[6];
    snprintf(id_str, sizeof(id_str), "%04d", (int)id);

    char path_buffer[256];
    snprintf(path_buffer, sizeof(path_buffer), "%s%s%s%s%s%s/default.png",
             LVGL_VFS_SD_CARD_PREFIX,
             SD_CARD_ROOT_PATH,
             ASSETS_BASE_SUBPATH,
             ASSETS_SPRITES_SUBPATH,
             SPRITES_PETS_SUBPATH,
             id_str);
    return std::string(path_buffer);
}

std::string PetManager::get_current_pet_sprite_path() const {
    if (s_pet_state.current_pet_id == PetId::NONE) {
        char path_buffer[256];
        snprintf(path_buffer, sizeof(path_buffer), "%s%s%s%s%s%s",
                 LVGL_VFS_SD_CARD_PREFIX,
                 SD_CARD_ROOT_PATH,
                 ASSETS_BASE_SUBPATH,
                 ASSETS_SPRITES_SUBPATH,
                 SPRITES_EGGS_SUBPATH,
                 DEFAULT_EGG_SPRITE);
        return std::string(path_buffer);
    }
    return get_sprite_path_for_id(s_pet_state.current_pet_id);
}

void PetManager::update_state() {
    if (!s_is_initialized) return;
    
    time_t now = time(NULL);
    if (now < SECONDS_IN_DAY) return; // Wait for valid time

    if (is_in_egg_stage()) {
        if (now >= s_pet_state.cycle_end_timestamp) {
            hatch_egg();
        }
        return;
    }

    if (now >= s_pet_state.cycle_end_timestamp) {
        ESP_LOGI(TAG, "Current pet cycle has ended.");
        finalize_cycle(); // Check final stage care points
        return;
    }

    const auto* current_data = get_pet_data(s_pet_state.current_pet_id);
    if (!current_data || current_data->evolves_to == PetId::NONE) {
        return; // No more evolutions, wait for finalize_cycle
    }

    time_t total_duration = s_pet_state.cycle_end_timestamp - s_pet_state.cycle_start_timestamp;
    if (total_duration <= 0) total_duration = 1;
    time_t elapsed_time = now - s_pet_state.cycle_start_timestamp;
    float progress = (float)elapsed_time / total_duration;

    PetId next_evolution_id = PetId::NONE;
    const auto* base_data = get_pet_data(s_pet_state.base_pet_id);

    if (s_pet_state.current_pet_id == s_pet_state.base_pet_id && progress >= STAGE_2_EVOLUTION_PERCENT) {
        next_evolution_id = current_data->evolves_to;
    } else if (base_data && s_pet_state.current_pet_id == base_data->evolves_to && progress >= STAGE_3_EVOLUTION_PERCENT) {
        next_evolution_id = current_data->evolves_to;
    }
    
    if (next_evolution_id != PetId::NONE) {
        ESP_LOGI(TAG, "Checking evolution for %s...", get_pet_name(s_pet_state.current_pet_id).c_str());
        if (s_pet_state.stage_care_points >= current_data->care_points_needed) {
            ESP_LOGI(TAG, "Success! Evolving into %s!", get_pet_name(next_evolution_id).c_str());
            s_pet_state.current_pet_id = next_evolution_id;
            s_pet_state.stage_care_points = 0; // Reset points for the new stage

            // Mark line as discovered
            for (auto& entry : s_collection) {
                if (entry.base_id == s_pet_state.base_pet_id && !entry.discovered) {
                    entry.discovered = true;
                    ESP_LOGI(TAG, "Marked %s line as discovered.", get_pet_name(entry.base_id).c_str());
                    save_collection();
                    break;
                }
            }
            save_state();
        } else {
            ESP_LOGW(TAG, "Failed to evolve. Needed %lu points, had %lu.", current_data->care_points_needed, s_pet_state.stage_care_points);
            fail_cycle();
        }
    }
}

void PetManager::start_new_cycle() {
    ESP_LOGI(TAG, "Starting a new egg cycle.");
    time_t now = time(NULL);

    s_pet_state.base_pet_id = select_random_hatchable_pet();
    s_pet_state.current_pet_id = PetId::NONE;
    s_pet_state.stage_care_points = 0;
    s_pet_state.custom_name.clear();
    
    s_pet_state.cycle_start_timestamp = now;
    s_pet_state.cycle_end_timestamp = now + EGG_HATCH_DURATION_SECONDS;
    
    ESP_LOGI(TAG, "New egg will hatch at timestamp %ld", (long)s_pet_state.cycle_end_timestamp);

    save_state();
}

void PetManager::hatch_egg() {
    ESP_LOGI(TAG, "Egg is hatching into %s!", get_pet_name(s_pet_state.base_pet_id).c_str());
    time_t now = time(NULL);

    s_pet_state.current_pet_id = s_pet_state.base_pet_id;
    s_pet_state.cycle_start_timestamp = now;
    s_pet_state.stage_care_points = 0;

#if PET_LIFECYCLE_DEBUG_7_MINUTES == 1
    s_pet_state.cycle_end_timestamp = now + (7 * 60);
    ESP_LOGW(TAG, "DEBUG LIFECYCLE ENABLED: Pet cycle will last 7 minutes.");
#else
    time_t target_end_date = now + (MIN_CYCLE_DURATION_DAYS * SECONDS_IN_DAY);
    struct tm timeinfo;
    localtime_r(&target_end_date, &timeinfo);
    int days_to_next_sunday = (7 - timeinfo.tm_wday) % 7;
    time_t sunday_end_date = target_end_date + (days_to_next_sunday * SECONDS_IN_DAY);
    localtime_r(&sunday_end_date, &timeinfo);
    timeinfo.tm_hour = 23; timeinfo.tm_min = 59; timeinfo.tm_sec = 59;
    s_pet_state.cycle_end_timestamp = mktime(&timeinfo);
#endif

    struct tm end_tm;
    localtime_r(&s_pet_state.cycle_end_timestamp, &end_tm);
    char end_buf[30];
    strftime(end_buf, sizeof(end_buf), "%Y-%m-%d %H:%M:%S", &end_tm);
    ESP_LOGI(TAG, "New pet's lifecycle ends on: %s", end_buf);

    save_state();
}

void PetManager::fail_cycle() {
    ESP_LOGW(TAG, "Pet cycle failed. The pet was not properly cared for.");
    // Even on failure, if we saw the pet, it should be marked as discovered.
    for (auto& entry : s_collection) {
        if (entry.base_id == s_pet_state.base_pet_id) {
            entry.discovered = true; // Mark as seen
            ESP_LOGI(TAG, "Marked %s line as discovered despite cycle failure.", get_pet_name(entry.base_id).c_str());
            save_collection();
            break;
        }
    }
    start_new_cycle();
}

void PetManager::finalize_cycle() {
    ESP_LOGI(TAG, "Finalizing cycle for %s. Stage Care Points: %lu", get_pet_name(s_pet_state.base_pet_id).c_str(), s_pet_state.stage_care_points);
    const auto* final_data = get_pet_data(s_pet_state.current_pet_id);
    
    if (final_data && final_data->evolves_to == PetId::NONE && s_pet_state.stage_care_points >= final_data->care_points_needed) {
        ESP_LOGI(TAG, "Success! Pet line collected!");
        for (auto& entry : s_collection) {
            if (entry.base_id == s_pet_state.base_pet_id) {
                entry.collected = true;
                entry.discovered = true;
                break;
            }
        }
        save_collection();
    } else {
        ESP_LOGW(TAG, "Failed to collect pet at final stage. It will be remembered only as 'discovered'.");
        // No need to call fail_cycle, as start_new_cycle will be called next.
        for (auto& entry : s_collection) {
            if (entry.base_id == s_pet_state.base_pet_id) {
                entry.discovered = true; // Ensure it's marked as discovered
                save_collection();
                break;
            }
        }
    }
    start_new_cycle();
}

void PetManager::add_care_points(uint32_t points) {
    if (is_in_egg_stage()) return; // Cannot care for an egg
    s_pet_state.stage_care_points += points;
    ESP_LOGI(TAG, "Added %lu care points. Stage Total: %lu", points, s_pet_state.stage_care_points);
    save_state();
}

void PetManager::force_new_cycle() {
    ESP_LOGI(TAG, "Forcing new pet cycle by user request.");
    start_new_cycle();
}

PetState PetManager::get_current_pet_state() const {
    return s_pet_state;
}

uint32_t PetManager::get_current_stage_care_goal() const {
    if (is_in_egg_stage()) {
        return 0;
    }
    if (const auto* data = get_pet_data(s_pet_state.current_pet_id)) {
        return data->care_points_needed;
    }
    return 100; // Default fallback
}

std::vector<PetCollectionEntry> PetManager::get_collection() const {
    return s_collection;
}

bool PetManager::is_in_egg_stage() const {
    return s_pet_state.current_pet_id == PetId::NONE;
}

time_t PetManager::get_time_to_hatch() const {
    if (!is_in_egg_stage()) return 0;
    time_t now = time(NULL);
    if (now >= s_pet_state.cycle_end_timestamp) return 0;
    return s_pet_state.cycle_end_timestamp - now;
}

time_t PetManager::get_time_to_next_stage(const PetState& state) const {
    if (state.current_pet_id == PetId::NONE) return 0;
    const auto* data = get_pet_data(state.current_pet_id);
    if (!data || data->evolves_to == PetId::NONE) {
        // For final stage, return time until cycle ends
        time_t now = time(NULL);
        if (now >= state.cycle_end_timestamp) return 0;
        return state.cycle_end_timestamp - now;
    }

    time_t now = time(NULL);
    if (now < SECONDS_IN_DAY) return 0; 

    time_t total_duration = state.cycle_end_timestamp - state.cycle_start_timestamp;
    if (total_duration <= 0) return 0;
    
    float next_stage_percent = 0.0f;
    if (state.current_pet_id == state.base_pet_id) {
        next_stage_percent = STAGE_2_EVOLUTION_PERCENT;
    } else {
        next_stage_percent = STAGE_3_EVOLUTION_PERCENT;
    }

    time_t next_stage_timestamp = state.cycle_start_timestamp + (time_t)(total_duration * next_stage_percent);

    return (next_stage_timestamp > now) ? (next_stage_timestamp - now) : 0;
}

void PetManager::load_state() {
    uint32_t u32_val;
    
    s_pet_state = {};
    if (data_manager_get_u32(PET_BASE_ID_KEY, &u32_val)) s_pet_state.base_pet_id = (PetId)u32_val;
    if (data_manager_get_u32(PET_CURRENT_ID_KEY, &u32_val)) s_pet_state.current_pet_id = (PetId)u32_val;
    if (data_manager_get_u32(PET_STAGE_POINTS_KEY, &s_pet_state.stage_care_points)) {}
    if (data_manager_get_u32(PET_START_TS_KEY, &u32_val)) s_pet_state.cycle_start_timestamp = (time_t)u32_val;
    if (data_manager_get_u32(PET_END_TS_KEY, &u32_val)) s_pet_state.cycle_end_timestamp = (time_t)u32_val;

    char name_buf[32] = {0};
    size_t name_len = sizeof(name_buf);
    if (data_manager_get_str(PET_NAME_KEY, name_buf, &name_len)) {
        s_pet_state.custom_name = name_buf;
    }

    s_collection.clear();
    for (const auto& pair : PET_DATA_REGISTRY) {
        if (pair.second.can_hatch) {
            PetCollectionEntry entry = { .base_id = pair.first, .discovered = false, .collected = false };
            std::string key_d = std::string(PET_COLL_PREFIX_KEY) + std::to_string((int)pair.first) + "_d";
            std::string key_c = std::string(PET_COLL_PREFIX_KEY) + std::to_string((int)pair.first) + "_c";
            uint32_t discovered = 0, collected = 0;
            data_manager_get_u32(key_d.c_str(), &discovered);
            data_manager_get_u32(key_c.c_str(), &collected);
            entry.discovered = (discovered == 1);
            entry.collected = (collected == 1);
            s_collection.push_back(entry);
        }
    }
    std::sort(s_collection.begin(), s_collection.end(), [](const PetCollectionEntry& a, const PetCollectionEntry& b) {
        return a.base_id < b.base_id;
    });

    if (s_pet_state.cycle_start_timestamp == 0 && time(NULL) > SECONDS_IN_DAY) {
        ESP_LOGI(TAG, "No cycle data found. Starting first cycle.");
        start_new_cycle();
    }
}

void PetManager::save_state() const {
    data_manager_set_u32(PET_BASE_ID_KEY, (uint32_t)s_pet_state.base_pet_id);
    data_manager_set_u32(PET_CURRENT_ID_KEY, (uint32_t)s_pet_state.current_pet_id);
    data_manager_set_u32(PET_STAGE_POINTS_KEY, s_pet_state.stage_care_points);
    data_manager_set_u32(PET_START_TS_KEY, (uint32_t)s_pet_state.cycle_start_timestamp);
    data_manager_set_u32(PET_END_TS_KEY, (uint32_t)s_pet_state.cycle_end_timestamp);
    data_manager_set_str(PET_NAME_KEY, s_pet_state.custom_name.c_str());
}

void PetManager::save_collection() const {
    for (const auto& entry : s_collection) {
        std::string key_d = std::string(PET_COLL_PREFIX_KEY) + std::to_string((int)entry.base_id) + "_d";
        std::string key_c = std::string(PET_COLL_PREFIX_KEY) + std::to_string((int)entry.base_id) + "_c";
        data_manager_set_u32(key_d.c_str(), entry.discovered ? 1 : 0);
        data_manager_set_u32(key_c.c_str(), entry.collected ? 1 : 0);
    }
}

PetId PetManager::get_final_evolution(PetId base_id) const {
    PetId current_id = base_id;
    while (true) {
        const auto* data = get_pet_data(current_id);
        if (!data || data->evolves_to == PetId::NONE) {
            return current_id;
        }
        current_id = data->evolves_to;
    }
}