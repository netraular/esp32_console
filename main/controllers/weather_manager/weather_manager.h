#ifndef WEATHER_MANAGER_H
#define WEATHER_MANAGER_H

#include <vector>
#include <string>
#include <time.h>

/**
 * @brief Data structure for a single forecast point.
 */
struct ForecastData {
    time_t timestamp;   // The time for this forecast point
    int weather_code;   // The WMO weather code from the API
};

/**
 * @brief Manages fetching and caching weather data from an online API.
 *
 * This class runs a background task to periodically fetch forecast data from
 * the Open-Meteo API. It provides a thread-safe way for the UI to access
 * the latest cached forecast.
 */
class WeatherManager {
public:
    /**
     * @brief Initializes the weather manager and starts its background fetch task.
     */
    static void init();

    /**
     * @brief Gets the latest cached weather forecast.
     *
     * @return A vector of ForecastData points for the configured hours.
     *         The vector may be empty if no data has been fetched yet.
     */
    static std::vector<ForecastData> get_forecast();

    /**
     * @brief Converts a WMO weather code into a corresponding LVGL symbol string.
     *
     * @param wmo_code The integer weather code from the API.
     * @return A const char* pointing to an LVGL symbol string (e.g., LV_SYMBOL_SUN).
     */
    static const char* wmo_code_to_lvgl_symbol(int wmo_code);

private:
    /**
     * @brief The FreeRTOS task that periodically fetches weather data.
     * @param pvParameters Unused.
     */
    static void weather_fetch_task(void* pvParameters);
};

#endif // WEATHER_MANAGER_H