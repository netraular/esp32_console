#include "habit_data_manager.h"
#include "controllers/littlefs_manager/littlefs_manager.h"
#include "esp_log.h"
#include <sstream>
#include <algorithm>
#include <string> // For std::string, std::stoul, etc.

static const char* TAG = "HABIT_DATA_MGR";

// --- File paths within LittleFS ---
static const char* DIR_PATH = "habits";
static const std::string CATEGORIES_FILENAME = std::string(DIR_PATH) + "/categories.csv";
static const std::string HABITS_FILENAME = std::string(DIR_PATH) + "/habits.csv";
static const std::string ID_COUNTER_FILENAME = std::string(DIR_PATH) + "/id.txt";
static const char* GENERAL_CATEGORY_NAME = "General";

// --- Static Member Initialization ---
std::vector<HabitCategory> HabitDataManager::s_categories;
std::vector<Habit> HabitDataManager::s_habits;
uint32_t HabitDataManager::s_next_id = 1;

void HabitDataManager::init() {
    ESP_LOGI(TAG, "Initializing Habit Data Manager...");
    if (!littlefs_manager_ensure_dir_exists(DIR_PATH)) {
        ESP_LOGE(TAG, "Failed to create habits directory! Data will not be loaded or saved.");
        return;
    }
    load_data();
}

void HabitDataManager::load_data() {
    s_categories.clear();
    s_habits.clear();

    // Load ID Counter
    char* id_buffer = nullptr;
    size_t id_size = 0;
    if (littlefs_manager_read_file(ID_COUNTER_FILENAME.c_str(), &id_buffer, &id_size) && id_buffer) {
        s_next_id = std::stoul(id_buffer);
        free(id_buffer);
        ESP_LOGI(TAG, "Loaded next_id: %lu", s_next_id);
    }

    // --- MODIFIED: Load Categories with is_deletable flag ---
    // CSV Format: id,is_active,is_deletable,name
    char* cat_buffer = nullptr;
    size_t cat_size = 0;
    if (littlefs_manager_read_file(CATEGORIES_FILENAME.c_str(), &cat_buffer, &cat_size) && cat_buffer) {
        std::stringstream ss(cat_buffer);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            std::stringstream line_ss(line);
            std::string id_str, active_str, deletable_str, name;
            if (std::getline(line_ss, id_str, ',') && 
                std::getline(line_ss, active_str, ',') &&
                std::getline(line_ss, deletable_str, ',') && 
                std::getline(line_ss, name)) {
                s_categories.push_back({(uint32_t)std::stoul(id_str), name, (bool)std::stoi(active_str), (bool)std::stoi(deletable_str)});
            }
        }
        free(cat_buffer);
        ESP_LOGI(TAG, "Loaded %d categories.", (int)s_categories.size());
    }

    // --- NEW: First-time initialization logic ---
    if (s_categories.empty()) {
        ESP_LOGI(TAG, "No categories found. Creating default 'General' category.");
        uint32_t new_id = get_next_unique_id();
        s_categories.push_back({new_id, GENERAL_CATEGORY_NAME, true, false}); // id, name, active, deletable=false
        save_categories(); // Save immediately
    }

    // Load Habits (unchanged)
    char* habit_buffer = nullptr;
    size_t habit_size = 0;
    if (littlefs_manager_read_file(HABITS_FILENAME.c_str(), &habit_buffer, &habit_size) && habit_buffer) {
        std::stringstream ss(habit_buffer);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            std::stringstream line_ss(line);
            std::string id_str, cat_id_str, active_str, color_hex, name;
            if (std::getline(line_ss, id_str, ',') && 
                std::getline(line_ss, cat_id_str, ',') && 
                std::getline(line_ss, active_str, ',') && 
                std::getline(line_ss, color_hex, ',') && 
                std::getline(line_ss, name)) {
                s_habits.push_back({(uint32_t)std::stoul(id_str), (uint32_t)std::stoul(cat_id_str), name, color_hex, (bool)std::stoi(active_str)});
            }
        }
        free(habit_buffer);
        ESP_LOGI(TAG, "Loaded %d habits.", (int)s_habits.size());
    }
}

// --- MODIFIED: Save Categories with is_deletable flag ---
// CSV Format: id,is_active,is_deletable,name
void HabitDataManager::save_categories() {
    std::stringstream ss;
    for (const auto& category : s_categories) {
        ss << category.id << "," << (int)category.is_active << "," << (int)category.is_deletable << "," << category.name << "\n";
    }
    if (!littlefs_manager_write_file(CATEGORIES_FILENAME.c_str(), ss.str().c_str())) {
        ESP_LOGE(TAG, "Failed to save categories!");
    }
}

// Save Habits (unchanged)
void HabitDataManager::save_habits() {
    std::stringstream ss;
    for (const auto& habit : s_habits) {
        ss << habit.id << "," << habit.category_id << "," << (int)habit.is_active << "," << habit.color_hex << "," << habit.name << "\n";
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

// --- Public API Methods (Implementations) ---

std::vector<HabitCategory> HabitDataManager::get_active_categories() {
    std::vector<HabitCategory> active_cats;
    for (const auto& cat : s_categories) {
        if (cat.is_active) {
            active_cats.push_back(cat);
        }
    }
    return active_cats;
}

HabitCategory* HabitDataManager::get_category_by_id(uint32_t category_id) {
    for (auto& cat : s_categories) {
        if (cat.id == category_id) {
            return &cat;
        }
    }
    return nullptr;
}

bool HabitDataManager::add_category(const std::string& name) {
    uint32_t new_id = get_next_unique_id();
    s_categories.push_back({new_id, name, true, true}); // User-added categories are always deletable
    save_categories();
    ESP_LOGI(TAG, "Added category '%s' with ID %lu", name.c_str(), new_id);
    return true;
}

bool HabitDataManager::archive_category(uint32_t category_id) {
    auto it = std::find_if(s_categories.begin(), s_categories.end(), [category_id](const HabitCategory& cat){
        return cat.id == category_id;
    });

    if (it != s_categories.end()) {
        // --- NEW: Check if the category is deletable ---
        if (!it->is_deletable) {
            ESP_LOGW(TAG, "Attempted to archive a non-deletable category (ID: %lu). Operation denied.", category_id);
            return false;
        }

        it->is_active = false;
        save_categories();
        // Also archive all habits in this category
        for (auto& habit : s_habits) {
            if (habit.category_id == category_id) {
                habit.is_active = false;
            }
        }
        save_habits();
        ESP_LOGI(TAG, "Archived category with ID %lu", category_id);
        return true;
    }
    ESP_LOGW(TAG, "Could not find category with ID %lu to archive", category_id);
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

std::vector<Habit> HabitDataManager::get_active_habits_for_category(uint32_t category_id) {
    std::vector<Habit> active_habits;
    for (const auto& habit : s_habits) {
        if (habit.category_id == category_id && habit.is_active) {
            active_habits.push_back(habit);
        }
    }
    return active_habits;
}

bool HabitDataManager::add_habit(const std::string& name, uint32_t category_id, const std::string& color_hex) {
    uint32_t new_id = get_next_unique_id();
    s_habits.push_back({new_id, category_id, name, color_hex, true});
    save_habits();
    ESP_LOGI(TAG, "Added habit '%s' with ID %lu, color %s", name.c_str(), new_id, color_hex.c_str());
    return true;
}

bool HabitDataManager::archive_habit(uint32_t habit_id) {
    auto it = std::find_if(s_habits.begin(), s_habits.end(), [habit_id](const Habit& h){
        return h.id == habit_id;
    });

    if (it != s_habits.end()) {
        it->is_active = false;
        save_habits();
        ESP_LOGI(TAG, "Archived habit with ID %lu", habit_id);
        return true;
    }
    ESP_LOGW(TAG, "Could not find habit with ID %lu to archive", habit_id);
    return false;
}

bool HabitDataManager::delete_habit_permanently(uint32_t habit_id) { 
    ESP_LOGW(TAG, "delete_habit_permanently not implemented");
    return false; 
}
bool HabitDataManager::mark_habit_as_done(uint32_t habit_id, time_t date) {
    ESP_LOGW(TAG, "mark_habit_as_done not implemented");
    return false;
}
HabitHistory HabitDataManager::get_history_for_habit(uint32_t habit_id) {
    ESP_LOGW(TAG, "get_history_for_habit not implemented");
    return {};
}