#ifndef HABIT_DATA_MANAGER_H
#define HABIT_DATA_MANAGER_H

#include "models/habit_data_models.h" // Include the data models
#include <string>
#include <vector>
#include <time.h>

/**
 * @brief Manages loading, saving, and accessing all habit-related data.
 *
 * This class acts as a centralized service for all habit data, abstracting
 * away the filesystem storage details from the UI views. It uses a soft-delete
 * pattern (is_active flag) for categories and habits to maintain historical data.
 */
class HabitDataManager {
public:
    /**
     * @brief Initializes the manager, loading all data from LittleFS into memory.
     * Must be called once at application startup.
     */
    static void init();

    // --- Category Management ---
    static std::vector<HabitCategory> get_active_categories();
    static HabitCategory* get_category_by_id(uint32_t category_id);
    static bool add_category(const std::string& name);
    static bool archive_category(uint32_t category_id);
    static int get_habit_count_for_category(uint32_t category_id, bool active_only);

    // --- Habit Management ---
    static std::vector<Habit> get_active_habits_for_category(uint32_t category_id);
    static std::vector<Habit> get_all_active_habits();
    static Habit* get_habit_by_id(uint32_t habit_id);
    static bool add_habit(const std::string& name, uint32_t category_id, const std::string& color_hex);
    static bool archive_habit(uint32_t habit_id);
    static bool delete_habit_permanently(uint32_t habit_id); // Optional: for permanent deletion

    // --- History Management ---
    static bool mark_habit_as_done(uint32_t habit_id, time_t date);
    static bool unmark_habit_as_done(uint32_t habit_id, time_t date);
    static bool is_habit_done_today(uint32_t habit_id);
    static HabitHistory get_history_for_habit(uint32_t habit_id);

private:
    // In-memory cache of all data
    static std::vector<HabitCategory> s_categories;
    static std::vector<Habit> s_habits;
    static uint32_t s_next_id;

    // Private methods for loading from and saving to LittleFS
    static void load_data();
    static void save_categories();
    static void save_habits();
    static void save_id_counter();

    static uint32_t get_next_unique_id();

    // --- NEW: History Helper Methods ---
    static std::string get_history_filepath(uint32_t habit_id);
    static std::vector<time_t> read_history_file(uint32_t habit_id);
    static bool write_history_file(uint32_t habit_id, const std::vector<time_t>& dates);
    static bool is_same_day(time_t t1, time_t t2);
};

#endif // HABIT_DATA_MANAGER_H