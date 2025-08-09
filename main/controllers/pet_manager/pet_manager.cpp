#include "pet_manager.h"
#include "controllers/data_manager/data_manager.h"
#include "models/pet_asset_data.h"
#include "models/asset_config.h" 
#include "esp_log.h"
#include "esp_random.h"
#include <algorithm>
#include <cstdio>

// --- DEBUG ---
// Set to 1 to enable a 7-minute pet lifecycle for rapid testing.
// Set to 0 or comment out for the standard, long lifecycle.
#define PET_LIFECYCLE_DEBUG_7_MINUTES 1

static const char* TAG = "PET_MGR";

// --- NVS Keys for Pet State Persistence ---
static const char* PET_STATE_TYPE_KEY = "pet_type";
static const char* PET_STATE_POINTS_KEY = "pet_points";
static const char* PET_STATE_NAME_KEY = "pet_name";
static const char* PET_STATE_START_TS_KEY = "pet_start_ts";
static const char* PET_STATE_END_TS_KEY = "pet_end_ts";
static const char* PET_COLL_PREFIX_KEY = "pet_coll_"; 

// --- Game Logic Constants ---
constexpr uint32_t CARE_POINTS_TO_COLLECT = 100;
constexpr time_t SECONDS_IN_DAY = 86400;
constexpr int MIN_CYCLE_DURATION_DAYS = 10; 

// --- Evolution Stage Time Percentages ---
constexpr float EGG_STAGE_END_PERCENT = 0.0714f; //1st day
constexpr float BABY_STAGE_END_PERCENT = 0.2857f; //4th day
constexpr float TEEN_STAGE_END_PERCENT = 0.5714f; //8th day

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

std::string PetManager::get_pet_display_name(const PetState& state) const {
    if (state.stage == PetStage::EGG) {
        return "Mysterious Egg";
    }
    if (!state.custom_name.empty() && state.custom_name != "pet_name") {
        return state.custom_name;
    }
    return this->get_pet_base_name(state.type);
}

std::string PetManager::get_pet_base_name(PetType type) const {
    if (type < PetType::COUNT) {
        return PET_ASSET_DEFINITIONS[(int)type].base_name;
    }
    return "Unknown";
}

PetType PetManager::select_random_new_pet() {
    std::vector<PetType> available_pets;
    for (const auto& entry : s_collection) {
        if (!entry.collected) {
            available_pets.push_back(entry.type);
        }
    }

    if (available_pets.empty()) {
        ESP_LOGI(TAG, "All pets collected! Resetting collection for replay.");
        for (auto& entry : s_collection) entry.collected = false;
        save_collection();
        for (int i = 0; i < (int)PetType::COUNT; ++i) available_pets.push_back((PetType)i);
    }
    
    uint32_t index = esp_random() % available_pets.size();
    PetType new_type = available_pets[index];

    ESP_LOGI(TAG, "Selected new pet: %s", get_pet_base_name(new_type).c_str());
    return new_type;
}

std::string PetManager::get_current_pet_sprite_path() const {
    const char* subfolder = nullptr;
    const char* filename = nullptr;

    if (s_pet_state.stage == PetStage::EGG) {
        subfolder = SPRITES_EGGS_SUBPATH;
        filename = DEFAULT_EGG_SPRITE;
    } else {
        subfolder = SPRITES_PETS_SUBPATH;
        
        const PetAssetData& pet_assets = PET_ASSET_DEFINITIONS[(int)s_pet_state.type];

        switch (s_pet_state.stage) {
            case PetStage::BABY:
                filename = pet_assets.baby_sprite_filename ? pet_assets.baby_sprite_filename : DEFAULT_BABY_SPRITE;
                break;
            case PetStage::TEEN:
                filename = pet_assets.teen_sprite_filename ? pet_assets.teen_sprite_filename : DEFAULT_TEEN_SPRITE;
                break;
            case PetStage::ADULT:
                filename = pet_assets.adult_sprite_filename ? pet_assets.adult_sprite_filename : DEFAULT_ADULT_SPRITE;
                break;
            default: // Should not happen
                return "";
        }
    }

    if (subfolder && filename) {
        char path_buffer[256];
        snprintf(path_buffer, sizeof(path_buffer), "%s%s%s%s%s%s",
                 LVGL_VFS_SD_CARD_PREFIX, // "S:"
                 SD_CARD_ROOT_PATH,       // "/sdcard"
                 ASSETS_BASE_SUBPATH,     // "/assets/"
                 ASSETS_SPRITES_SUBPATH,  // "sprites/"
                 subfolder,               // "pets/" or "eggs/"
                 filename);               // "fluffle_adult.png"
        return std::string(path_buffer);
    }

    return ""; // Return empty path on failure
}

void PetManager::update_state() {
    if (!s_is_initialized) {
        ESP_LOGE(TAG, "Manager not initialized.");
        return;
    }
    
    time_t now = time(NULL);
    if (now < SECONDS_IN_DAY) {
        ESP_LOGW(TAG, "Time is not synced, cannot update pet state.");
        return;
    }

    // Check if the current cycle has ended
    if (now >= s_pet_state.cycle_end_timestamp) {
        ESP_LOGI(TAG, "Current pet cycle has ended.");
        finalize_cycle();
    }

    // --- Pet Stage Calculation ---
    time_t total_duration = s_pet_state.cycle_end_timestamp - s_pet_state.cycle_start_timestamp;
    if (total_duration <= 0) total_duration = 1; // Prevent division by zero
    
    time_t elapsed_time = now - s_pet_state.cycle_start_timestamp;
    
    float progress = (float)elapsed_time / total_duration;

    PetStage new_stage;
    if (progress < EGG_STAGE_END_PERCENT)           new_stage = PetStage::EGG;
    else if (progress < BABY_STAGE_END_PERCENT)     new_stage = PetStage::BABY;
    else if (progress < TEEN_STAGE_END_PERCENT)     new_stage = PetStage::TEEN;
    else                                            new_stage = PetStage::ADULT;

    if (s_pet_state.stage != new_stage) {
        ESP_LOGI(TAG, "Pet stage changed to %d (Progress: %.2f%%)", (int)new_stage, progress * 100);
        s_pet_state.stage = new_stage;
        
        if (new_stage == PetStage::TEEN) {
            s_collection[(int)s_pet_state.type].discovered = true;
            save_collection();
        }
        save_state();
    }
}

void PetManager::finalize_cycle() {
    ESP_LOGI(TAG, "Finalizing cycle for pet type %d. Care points: %lu", (int)s_pet_state.type, s_pet_state.care_points);
    if (s_pet_state.stage == PetStage::ADULT && s_pet_state.care_points >= CARE_POINTS_TO_COLLECT) {
        ESP_LOGI(TAG, "Success! Pet collected!");
        s_collection[(int)s_pet_state.type].collected = true;
    } else {
        ESP_LOGW(TAG, "Failed to collect pet. It will be remembered only as 'discovered'.");
    }
    save_collection();
    start_new_cycle();
}

void PetManager::start_new_cycle() {
    ESP_LOGI(TAG, "Starting a new pet cycle.");
    time_t now = time(NULL);

    s_pet_state.type = select_random_new_pet();
    s_pet_state.stage = PetStage::EGG;
    s_pet_state.care_points = 0;
    s_pet_state.custom_name = "pet_name";
    s_pet_state.cycle_start_timestamp = now;

#if PET_LIFECYCLE_DEBUG_7_MINUTES == 1
    // --- DEBUG: 7-minute lifecycle ---
    constexpr time_t FIVE_MINUTES_IN_SECONDS = 7 * 60;
    s_pet_state.cycle_end_timestamp = now + FIVE_MINUTES_IN_SECONDS;
    ESP_LOGW(TAG, "DEBUG LIFECYCLE ENABLED: Pet cycle will last 5 minutes.");

#else
    // --- PRODUCTION: Standard long lifecycle ---
    time_t target_end_date = now + (MIN_CYCLE_DURATION_DAYS * SECONDS_IN_DAY);
    struct tm timeinfo_prod; // Use a different name to avoid shadowing
    localtime_r(&target_end_date, &timeinfo_prod);
    
    int days_to_next_sunday = (7 - timeinfo_prod.tm_wday) % 7;
    s_pet_state.cycle_end_timestamp = target_end_date + (days_to_next_sunday * SECONDS_IN_DAY);
    
    localtime_r(&s_pet_state.cycle_end_timestamp, &timeinfo_prod);
    timeinfo_prod.tm_hour = 23;
    timeinfo_prod.tm_min = 59;
    timeinfo_prod.tm_sec = 59;
    s_pet_state.cycle_end_timestamp = mktime(&timeinfo_prod);
#endif

    struct tm timeinfo;
    localtime_r(&s_pet_state.cycle_end_timestamp, &timeinfo);
    char end_buf[30];
    strftime(end_buf, sizeof(end_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "New cycle ends on: %s", end_buf);

    save_state();
}

void PetManager::add_care_points(uint32_t points) {
    s_pet_state.care_points += points;
    ESP_LOGI(TAG, "Added %lu care points. Total: %lu", points, s_pet_state.care_points);
    save_state();
}

void PetManager::force_new_cycle() {
    ESP_LOGI(TAG, "Forcing new pet cycle by user request.");
    // Simply start a new cycle. This overwrites the old pet's state.
    // finalize_cycle() is NOT called, so the pet is not marked as collected.
    start_new_cycle();
}

PetState PetManager::get_current_pet_state() const {
    return s_pet_state;
}

std::vector<PetCollectionEntry> PetManager::get_collection() const {
    return s_collection;
}

time_t PetManager::get_time_to_next_stage(const PetState& state) const {
    time_t now = time(NULL);
    if (now < SECONDS_IN_DAY) return 0; // Time not synced

    time_t total_duration = state.cycle_end_timestamp - state.cycle_start_timestamp;
    if (total_duration <= 0) return 0;

    float next_stage_percent = 0.0f;
    switch(state.stage) {
        case PetStage::EGG:  next_stage_percent = EGG_STAGE_END_PERCENT; break;
        case PetStage::BABY: next_stage_percent = BABY_STAGE_END_PERCENT; break;
        case PetStage::TEEN: next_stage_percent = TEEN_STAGE_END_PERCENT; break;
        case PetStage::ADULT:
        default:
            return 0; // At final stage or unknown
    }
    
    time_t next_stage_timestamp = state.cycle_start_timestamp + (time_t)(total_duration * next_stage_percent);

    if (next_stage_timestamp > now) {
        return next_stage_timestamp - now;
    }
    
    return 0;
}

void PetManager::load_state() {
    uint32_t u32_val;
    
    if (data_manager_get_u32(PET_STATE_TYPE_KEY, &u32_val)) {
        s_pet_state.type = (PetType)u32_val;
    }
    if (data_manager_get_u32(PET_STATE_POINTS_KEY, &s_pet_state.care_points)) {
    }
    if (data_manager_get_u32(PET_STATE_START_TS_KEY, &u32_val)) {
        s_pet_state.cycle_start_timestamp = (time_t)u32_val;
    }
    if (data_manager_get_u32(PET_STATE_END_TS_KEY, &u32_val)) {
        s_pet_state.cycle_end_timestamp = (time_t)u32_val;
    }

    char name_buf[32] = {0};
    size_t name_len = sizeof(name_buf);
    if (data_manager_get_str(PET_STATE_NAME_KEY, name_buf, &name_len)) {
        s_pet_state.custom_name = name_buf;
    } else {
        s_pet_state.custom_name.clear();
    }

    s_collection.resize((int)PetType::COUNT);
    for (int i = 0; i < (int)PetType::COUNT; ++i) {
        s_collection[i].type = (PetType)i;
        std::string key_d = std::string(PET_COLL_PREFIX_KEY) + std::to_string(i) + "_d";
        std::string key_c = std::string(PET_COLL_PREFIX_KEY) + std::to_string(i) + "_c";
        uint32_t discovered = 0, collected = 0;
        data_manager_get_u32(key_d.c_str(), &discovered);
        data_manager_get_u32(key_c.c_str(), &collected);
        s_collection[i].discovered = (discovered == 1);
        s_collection[i].collected = (collected == 1);
    }

    if (s_pet_state.cycle_end_timestamp == 0 && time(NULL) > SECONDS_IN_DAY) {
        ESP_LOGI(TAG, "No cycle data found. Starting first cycle.");
        start_new_cycle();
    }
}

void PetManager::save_state() const {
    data_manager_set_u32(PET_STATE_TYPE_KEY, (uint32_t)s_pet_state.type);
    data_manager_set_u32(PET_STATE_POINTS_KEY, s_pet_state.care_points);
    data_manager_set_u32(PET_STATE_START_TS_KEY, (uint32_t)s_pet_state.cycle_start_timestamp);
    data_manager_set_u32(PET_STATE_END_TS_KEY, (uint32_t)s_pet_state.cycle_end_timestamp);
    data_manager_set_str(PET_STATE_NAME_KEY, s_pet_state.custom_name.c_str());
}

void PetManager::save_collection() const {
    for (int i = 0; i < (int)PetType::COUNT; ++i) {
        std::string key_d = std::string(PET_COLL_PREFIX_KEY) + std::to_string(i) + "_d";
        std::string key_c = std::string(PET_COLL_PREFIX_KEY) + std::to_string(i) + "_c";
        data_manager_set_u32(key_d.c_str(), s_collection[i].discovered ? 1 : 0);
        data_manager_set_u32(key_c.c_str(), s_collection[i].collected ? 1 : 0);
    }
}