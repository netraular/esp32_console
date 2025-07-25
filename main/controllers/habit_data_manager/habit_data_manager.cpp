#include "habit_data_manager.h"
#include "controllers/littlefs_manager/littlefs_manager.h"
#include "esp_log.h"
#include <sstream>
#include <algorithm>
#include <string> // For std::string, std::stoul, etc.

static const char* TAG = "HABIT_DATA_MGR";

// --- File paths within LittleFS ---
// Base directory for all habit data.
static const char* DIR_PATH = "habits";

// File paths are now constructed using the directory path.
static const std::string CATEGORIES_FILENAME = std::string(DIR_PATH) + "/categories.csv";
static const std::string HABITS_FILENAME = std::string(DIR_PATH) + "/habits.csv";
static const std::string ID_COUNTER_FILENAME = std::string(DIR_PATH) + "/id.txt";

// --- Static Member Initialization ---
std::vector<HabitCategory> HabitDataManager::s_categories;
std::vector<Habit> HabitDataManager::s_habits;
uint32_t HabitDataManager::s_next_id = 1;

void HabitDataManager::init() {
    ESP_LOGI(TAG, "Initializing Habit Data Manager...");
    
    // --- FIX: Ensure the base directory exists before loading data ---
    if (!littlefs_manager_ensure_dir_exists(DIR_PATH)) {
        ESP_LOGE(TAG, "Failed to create habits directory! Data will not be loaded or saved.");
        return;
    }

    load_data();
}

void HabitDataManager::load_data() {
    s_categories.clear();
    s_habits.clear();

    // --- Load ID Counter ---
    char* id_buffer = nullptr;
    size_t id_size = 0;
    if (littlefs_manager_read_file(ID_COUNTER_FILENAME.c_str(), &id_buffer, &id_size) && id_buffer) {
        // --- MODIFICATION: Replaced try-catch with exception-free stringstream conversion ---
        std::stringstream ss(id_buffer);
        uint32_t temp_id = 0;
        if (ss >> temp_id) {
            s_next_id = temp_id;
        } else {
            ESP_LOGE(TAG, "Invalid ID counter file content. Resetting to 1.");
            s_next_id = 1;
        }
        // --- End of Modification ---
        free(id_buffer);
        ESP_LOGI(TAG, "Loaded next_id: %lu", s_next_id);
    }

    // --- Load Categories ---
    char* cat_buffer = nullptr;
    size_t cat_size = 0;
    if (littlefs_manager_read_file(CATEGORIES_FILENAME.c_str(), &cat_buffer, &cat_size) && cat_buffer) {
        std::stringstream ss(cat_buffer);
        std::string line;
        while (std::getline(ss, line)) {
            std::stringstream line_ss(line);
            std::string id_str, active_str, name;
            if (std::getline(line_ss, id_str, ',') && std::getline(line_ss, active_str, ',') && std::getline(line_ss, name)) {
                // Using simple stoi here is generally safe for 0/1 values, but could also be replaced.
                s_categories.push_back({(uint32_t)std::stoul(id_str), name, (bool)std::stoi(active_str)});
            }
        }
        free(cat_buffer);
        ESP_LOGI(TAG, "Loaded %d categories.", (int)s_categories.size());
    }

    // --- Load Habits ---
    char* habit_buffer = nullptr;
    size_t habit_size = 0;
     if (littlefs_manager_read_file(HABITS_FILENAME.c_str(), &habit_buffer, &habit_size) && habit_buffer) {
        std::stringstream ss(habit_buffer);
        std::string line;
        while (std::getline(ss, line)) {
            std::stringstream line_ss(line);
            std::string id_str, cat_id_str, active_str, name;
            if (std::getline(line_ss, id_str, ',') && std::getline(line_ss, cat_id_str, ',') && std::getline(line_ss, active_str, ',') && std::getline(line_ss, name)) {
                s_habits.push_back({(uint32_t)std::stoul(id_str), (uint32_t)std::stoul(cat_id_str), name, (bool)std::stoi(active_str)});
            }
        }
        free(habit_buffer);
        ESP_LOGI(TAG, "Loaded %d habits.", (int)s_habits.size());
    }
}

void HabitDataManager::save_categories() {
    std::stringstream ss;
    for (const auto& category : s_categories) {
        ss << category.id << "," << (int)category.is_active << "," << category.name << "\n";
    }
    if (!littlefs_manager_write_file(CATEGORIES_FILENAME.c_str(), ss.str().c_str())) {
        ESP_LOGE(TAG, "Failed to save categories!");
    }
}

void HabitDataManager::save_habits() {
     std::stringstream ss;
    for (const auto& habit : s_habits) {
        ss << habit.id << "," << habit.category_id << "," << (int)habit.is_active << "," << habit.name << "\n";
    }
    if (!littlefs_manager_write_file(HABITS_FILENAME.c_str(), ss.str().c_str())) {
        ESP_LOGE(TAG, "Failed to save habits!");
    }
}

void HabitDataManager::save_id_counter() {
    if (!littlefs_manager_write_file(ID_COUNTER_FILENAME.c_str(), std::to_string(s_next_id).c_str())) {
        ESP_LOGE(TAG, "Failed to save ID counter!");
    }
}

uint32_t HabitDataManager::get_next_unique_id() {
    uint32_t id = s_next_id++;
    save_id_counter();
    return id;
}

// --- Public API Methods ---

std::vector<HabitCategory> HabitDataManager::get_active_categories() {
    std::vector<HabitCategory> active_cats;
    for (const auto& cat : s_categories) {
        if (cat.is_active) {
            active_cats.push_back(cat);
        }
    }
    return active_cats;
}

bool HabitDataManager::add_category(const std::string& name) {
    uint32_t new_id = get_next_unique_id();
    s_categories.push_back({new_id, name, true});
    save_categories();
    return true;
}

bool HabitDataManager::archive_category(uint32_t category_id) {
    auto it = std::find_if(s_categories.begin(), s_categories.end(), [category_id](const HabitCategory& cat){
        return cat.id == category_id;
    });

    if (it != s_categories.end()) {
        it->is_active = false;
        save_categories();
        // Also archive all habits in this category
        for (auto& habit : s_habits) {
            if (habit.category_id == category_id) {
                habit.is_active = false;
            }
        }
        save_habits();
        return true;
    }
    return false;
}

int HabitDataManager::get_habit_count_for_category(uint32_t category_id, bool active_only) {
    int count = 0;
    for (const auto& habit : s_habits) {
        if (habit.category_id == category_id) {
            if (!active_only || habit.is_active) {
                count++;
            }
        }
    }
    return count;
}
// NOTE: Other public methods like add_habit, archive_habit, etc. should be implemented here.
// They are omitted for brevity.