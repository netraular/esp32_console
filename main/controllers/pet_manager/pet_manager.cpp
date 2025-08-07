#include "pet_manager.h"
#include "controllers/data_manager/data_manager.h"
#include "esp_log.h"
#include "esp_random.h"
#include <algorithm>

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
constexpr int MIN_CYCLE_DURATION_DAYS = 13; // The cycle will last at least this many days

// --- Evolution Stage Time Percentages ---
// These define what percentage of the total cycle duration each stage lasts.
constexpr float EGG_STAGE_END_PERCENT = 0.07f;  // Egg lasts for 7% of the cycle
constexpr float BABY_STAGE_END_PERCENT = 0.35f; // Baby lasts from 7% to 35% (28% of total)
constexpr float TEEN_STAGE_END_PERCENT = 0.70f; // Teen lasts from 35% to 70% (35% of total)
// Adult is from 70% to 100% (30% of total)

// --- Pet Name Data ---
const char* PET_NAMES[] = {"Fluffle", "Rocky", "Sprout", "Ember", "Aquabub"};

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
        // After finalizing, the state is already updated for the new cycle.
    }

    // --- Pet Stage Calculation ---
    // The current stage is determined by calculating the percentage of the cycle
    // that has already passed. This makes the evolution proportional and fair,
    // regardless of when the cycle started.

    // 1. Calculate the total duration of the current cycle in seconds.
    time_t total_duration = s_pet_state.cycle_end_timestamp - s_pet_state.cycle_start_timestamp;
    if (total_duration <= 0) total_duration = 1; // Prevent division by zero
    
    // 2. Calculate how much time has passed since the cycle began.
    time_t elapsed_time = now - s_pet_state.cycle_start_timestamp;
    
    // 3. Find the progress as a percentage (a float from 0.0 to 1.0).
    float progress = (float)elapsed_time / total_duration;

    // 4. Compare the progress against the predefined stage percentages.
    PetStage new_stage;
    if (progress < EGG_STAGE_END_PERCENT)           new_stage = PetStage::EGG;
    else if (progress < BABY_STAGE_END_PERCENT)     new_stage = PetStage::BABY;
    else if (progress < TEEN_STAGE_END_PERCENT)     new_stage = PetStage::TEEN;
    else                                            new_stage = PetStage::ADULT;

    if (s_pet_state.stage != new_stage) {
        ESP_LOGI(TAG, "Pet stage changed to %d (Progress: %.2f%%)", (int)new_stage, progress * 100);
        s_pet_state.stage = new_stage;
        
        // Mark as discovered when it reaches Teen stage
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
    s_pet_state.custom_name = "pet_name"; // Set default name
    s_pet_state.cycle_start_timestamp = now;

    // --- Cycle End Date Calculation ---
    // The end date is calculated to ensure two things:
    // 1. The cycle lasts for at least MIN_CYCLE_DURATION_DAYS.
    // 2. The cycle always ends at 23:59:59 on a Sunday.
    
    // First, find a date that is at least X days in the future.
    time_t target_end_date = now + (MIN_CYCLE_DURATION_DAYS * SECONDS_IN_DAY);
    struct tm timeinfo;
    localtime_r(&target_end_date, &timeinfo);
    
    // Then, find how many days we need to add to reach the *next* Sunday.
    // (tm_wday is 0 for Sunday, so (7 - 0) % 7 = 0; (7-1)%7 = 6 for Monday, etc.)
    int days_to_next_sunday = (7 - timeinfo.tm_wday) % 7;
    s_pet_state.cycle_end_timestamp = target_end_date + (days_to_next_sunday * SECONDS_IN_DAY);
    
    // Finally, set the time to the very end of that Sunday.
    localtime_r(&s_pet_state.cycle_end_timestamp, &timeinfo);
    timeinfo.tm_hour = 23;
    timeinfo.tm_min = 59;
    timeinfo.tm_sec = 59;
    s_pet_state.cycle_end_timestamp = mktime(&timeinfo);

    char end_buf[30];
    strftime(end_buf, sizeof(end_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "New cycle ends on: %s", end_buf);

    save_state();
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
        // Refill the list
        for (int i = 0; i < (int)PetType::COUNT; ++i) available_pets.push_back((PetType)i);
    }
    
    uint32_t index = esp_random() % available_pets.size();
    PetType new_type = available_pets[index];
    ESP_LOGI(TAG, "Selected new pet: %s", PET_NAMES[(int)new_type]);
    return new_type;
}

void PetManager::add_care_points(uint32_t points) {
    if (s_pet_state.stage > PetStage::EGG) {
        s_pet_state.care_points += points;
        ESP_LOGI(TAG, "Added %lu care points. Total: %lu", points, s_pet_state.care_points);
        save_state();
    } else {
        ESP_LOGI(TAG, "Cannot add care points to an egg.");
    }
}

PetState PetManager::get_current_pet_state() const {
    return s_pet_state;
}

std::vector<PetCollectionEntry> PetManager::get_collection() const {
    return s_collection;
}

std::string PetManager::get_pet_display_name(const PetState& state) const {
    if (state.stage == PetStage::EGG) {
        return "Mysterious Egg";
    }
    if (!state.custom_name.empty()) {
        return state.custom_name;
    }
    // Fallback to generic name if custom_name is somehow empty
    return PET_NAMES[(int)state.type];
}

std::string PetManager::get_pet_base_name(PetType type) const {
    if (type < PetType::COUNT) {
        return PET_NAMES[(int)type];
    }
    return "Unknown";
}

void PetManager::load_state() {
    uint32_t u32_val;
    
    if (data_manager_get_u32(PET_STATE_TYPE_KEY, &u32_val)) {
        s_pet_state.type = (PetType)u32_val;
    }
    if (data_manager_get_u32(PET_STATE_POINTS_KEY, &s_pet_state.care_points)) {
        // This body is intentionally empty, the value is loaded via the pointer.
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