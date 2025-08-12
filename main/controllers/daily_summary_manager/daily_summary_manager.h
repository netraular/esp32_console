#ifndef DAILY_SUMMARY_MANAGER_H
#define DAILY_SUMMARY_MANAGER_H

#include "models/daily_summary_model.h"
#include <string>
#include <time.h>
#include <functional> // For std::function

/**
 * @brief Manages loading, saving, and accessing daily summary data.
 *
 * This class acts as a service for all daily summary data, abstracting
 * the filesystem storage details. It stores one JSON file per day in LittleFS.
 */
class DailySummaryManager {
public:
    /**
     * @brief Initializes the manager, ensuring the base directory exists.
     */
    static void init();

    /**
     * @brief Retrieves the summary data for a specific date.
     * If no data file exists for the date, an empty summary is returned.
     * @param date The date to get the summary for. The time component is ignored.
     * @return A DailySummaryData struct for the requested day.
     */
    static DailySummaryData get_summary_for_date(time_t date);

    /**
     * @brief Gets the timestamp for the most recent day that has a summary file.
     * @return The timestamp for the latest summary, or 0 if none exist.
     */
    static time_t get_latest_summary_date();

    /**
     * @brief Adds a completed habit ID to the summary for a given date.
     * @param date The date of completion.
     * @param habit_id The ID of the completed habit.
     */
    static void add_completed_habit(time_t date, uint32_t habit_id);
    
    /**
     * @brief Removes a completed habit ID from the summary for a given date.
     * @param date The date of completion.
     * @param habit_id The ID of the habit to remove.
     */
    static void remove_completed_habit(time_t date, uint32_t habit_id);

    /**
     * @brief Sets the path for the daily journal entry for a given date.
     * @param date The date of the journal entry.
     * @param path The full path to the .wav file.
     */
    static void set_journal_path(time_t date, const std::string& path);
    
    /**
     * @brief Adds a voice note path to the summary for a given date.
     * @param date The date the note was created.
     * @param path The full path to the .wav file.
     */
    static void add_voice_note_path(time_t date, const std::string& path);
    
    /**
     * @brief Sets a callback to be invoked when summary data is changed.
     *
     * The active view (e.g., DailySummaryView) should register itself here to be
     * notified of external changes (e.g., a new voice note being saved).
     * @param cb The callback function. It receives the timestamp of the day that was modified.
     */
    static void set_on_data_changed_callback(std::function<void(time_t)> cb);

private:
    static time_t get_start_of_day(time_t timestamp);
    static std::string get_filepath_for_date(time_t date);
    static bool save_summary(const DailySummaryData& summary);

    static std::function<void(time_t)> on_data_changed_callback;
};

#endif // DAILY_SUMMARY_MANAGER_H