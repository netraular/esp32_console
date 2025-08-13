#ifndef DAILY_SUMMARY_MODEL_H
#define DAILY_SUMMARY_MODEL_H

#include <string>
#include <vector>
#include <time.h>
#include <cstdint>

/**
 * @brief Represents a summary of all tracked activities for a single day.
 *
 * This struct is designed to be serialized to/from JSON for persistent storage.
 */
struct DailySummaryData {
    time_t date; // Timestamp representing the start of the day (midnight).
    std::string journal_entry_path;
    std::vector<uint32_t> completed_habit_ids;
    std::vector<std::string> voice_note_paths;
    uint32_t pomodoro_work_seconds = 0; // Total seconds of completed Pomodoro work sessions.
};

#endif // DAILY_SUMMARY_MODEL_H