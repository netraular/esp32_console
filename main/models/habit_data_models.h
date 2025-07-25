// main/models/habit_data_models.h (Modificado)

#include <string>
#include <vector>
#include <time.h>

struct HabitCategory {
    uint32_t id;
    std::string name;
    bool is_active;
};

struct Habit {
    uint32_t id;
    uint32_t category_id;
    std::string name;
    bool is_active;

// HabitHistory no necesita cambios.
struct HabitHistory {
    uint32_t habit_id;
    std::vector<time_t> completed_dates;
};