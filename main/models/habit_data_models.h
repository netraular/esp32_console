#ifndef HABIT_DATA_MODELS_H
#define HABIT_DATA_MODELS_H

#include <string>
#include <vector>
#include <time.h>

// Represents a category for grouping habits.
struct HabitCategory {
    uint32_t id;
    std::string name;
    bool is_active;
    bool is_deletable; // System-defined categories like 'General' cannot be deleted.
};

// Represents a single trackable habit.
struct Habit {
    uint32_t id;
    uint32_t category_id;
    std::string name;
    std::string color_hex; // Color for the icon, e.g., "#FF5733"
    bool is_active;
};

// Represents the completion history for a single habit.
struct HabitHistory {
    uint32_t habit_id;
    std::vector<time_t> completed_dates;
};

#endif // HABIT_DATA_MODELS_H