#include "weather_manager.h"
#include "config/app_config.h" // Include application configuration
#include "controllers/wifi_manager/wifi_manager.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h" // For LV_SYMBOL_* defines
#include <string>

static const char* TAG = "WEATHER_MGR";

// --- Static Members ---
static std::vector<ForecastData> s_cached_forecast;
static SemaphoreHandle_t s_data_mutex = NULL;
extern const char open_meteo_ca_pem_start[] asm("_binary_open_meteo_ca_pem_start");

/**
 * @brief A context struct to hold state for a single HTTP request,
 *        used by the event handler.
 */
struct WeatherRequestContext {
    std::string response_buffer;
};

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    auto* context = static_cast<WeatherRequestContext*>(evt->user_data);
    if (!context) {
        return ESP_FAIL;
    }
    
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            context->response_buffer.append(static_cast<const char*>(evt->data), evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

void WeatherManager::weather_fetch_task(void* pvParameters) {
    for (;;) {
        ESP_LOGI(TAG, "Waiting for WiFi and Time Sync...");
        xEventGroupWaitBits(wifi_manager_get_event_group(), WIFI_CONNECTED_BIT | TIME_SYNC_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Network ready. Fetching weather data.");

        WeatherRequestContext context;
        esp_http_client_handle_t client = nullptr;

        esp_http_client_config_t config = {};
        config.url = WEATHER_API_URL; // Use the constant from app_config.h
        config.event_handler = _http_event_handler;
        config.user_data = &context;
        config.timeout_ms = 15000;
        config.cert_pem = open_meteo_ca_pem_start;
        config.method = HTTP_METHOD_GET;

        client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to initialize HTTP client. Retrying in %d minutes.", WEATHER_FETCH_INTERVAL_MS / 60000);
            vTaskDelay(pdMS_TO_TICKS(WEATHER_FETCH_INTERVAL_MS));
            continue;
        }

        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "HTTP GET request successful. Status = %d, content_length = %lld",
                     status_code, esp_http_client_get_content_length(client));

            if (status_code == 200) {
                cJSON *root = cJSON_Parse(context.response_buffer.c_str());
                if (root) {
                    cJSON *hourly = cJSON_GetObjectItem(root, "hourly");
                    if (hourly) {
                        cJSON *time_array = cJSON_GetObjectItem(hourly, "time");
                        cJSON *wcode_array = cJSON_GetObjectItem(hourly, "weather_code");

                        if (cJSON_IsArray(time_array) && cJSON_IsArray(wcode_array)) {
                            std::vector<ForecastData> new_forecast;
                            time_t now = time(NULL);
                            struct tm timeinfo;
                            localtime_r(&now, &timeinfo);
                            int current_hour = timeinfo.tm_hour;
                            int current_hour_idx = -1;

                            for (int i = 0; i < cJSON_GetArraySize(time_array); i++) {
                                const char* time_str = cJSON_GetArrayItem(time_array, i)->valuestring;
                                if (time_str && strstr(time_str, "T")) {
                                    if (atoi(strstr(time_str, "T") + 1) == current_hour) {
                                        current_hour_idx = i;
                                        break;
                                    }
                                }
                            }

                            if (current_hour_idx != -1) {
                                int hours_to_get[] = {1, 8, 12};
                                for (int h : hours_to_get) {
                                    int forecast_idx = current_hour_idx + h;
                                    if (forecast_idx < cJSON_GetArraySize(wcode_array) && forecast_idx < cJSON_GetArraySize(time_array)) {
                                        ForecastData fd;
                                        fd.weather_code = cJSON_GetArrayItem(wcode_array, forecast_idx)->valueint;
                                        
                                        struct tm forecast_tm = timeinfo;
                                        forecast_tm.tm_hour += h;
                                        fd.timestamp = mktime(&forecast_tm);
                                        new_forecast.push_back(fd);
                                    }
                                }

                                if (!new_forecast.empty()) {
                                    ESP_LOGI(TAG, "--- Parsed Weather Forecast ---");
                                    for(const auto& fd : new_forecast) {
                                        struct tm forecast_tm;
                                        localtime_r(&fd.timestamp, &forecast_tm);
                                        char time_buf[20];
                                        strftime(time_buf, sizeof(time_buf), "%H:00", &forecast_tm);
                                        ESP_LOGI(TAG, "  - Time: %s, Weather Code: %d, Symbol: %s", time_buf, fd.weather_code, wmo_code_to_lvgl_symbol(fd.weather_code));
                                    }
                                    ESP_LOGI(TAG, "-------------------------------");
                                    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
                                    s_cached_forecast = new_forecast;
                                    xSemaphoreGive(s_data_mutex);
                                } else {
                                     ESP_LOGW(TAG, "Parsing resulted in an empty forecast list.");
                                }
                            } else {
                                ESP_LOGE(TAG, "Could not find current hour in API response.");
                            }
                        } else {
                            ESP_LOGE(TAG, "Could not parse 'time' or 'weather_code' arrays from JSON.");
                        }
                    } else {
                        ESP_LOGE(TAG, "JSON response missing 'hourly' object.");
                    }
                    cJSON_Delete(root);
                } else {
                     ESP_LOGE(TAG, "Failed to parse JSON response. Response was: %s", context.response_buffer.c_str());
                }
            } else {
                 ESP_LOGE(TAG, "HTTP request failed with status code: %d. Body: %s", status_code, context.response_buffer.c_str());
            }
        } else {
            ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
        
        ESP_LOGI(TAG, "Weather task sleeping for %d minutes.", WEATHER_FETCH_INTERVAL_MS / 60000);
        vTaskDelay(pdMS_TO_TICKS(WEATHER_FETCH_INTERVAL_MS));
    }
    vTaskDelete(NULL);
}

void WeatherManager::init() {
    s_data_mutex = xSemaphoreCreateMutex();
    if (s_data_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create data mutex!");
        return;
    }
    xTaskCreate(weather_fetch_task, "weather_task", 5120, NULL, 5, NULL);
}

std::vector<ForecastData> WeatherManager::get_forecast() {
    std::vector<ForecastData> forecast_copy;
    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        forecast_copy = s_cached_forecast;
        xSemaphoreGive(s_data_mutex);
    }
    return forecast_copy;
}

const char* WeatherManager::wmo_code_to_lvgl_symbol(int wmo_code) {
    switch (wmo_code) {
        case 0: return LV_SYMBOL_SETTINGS; // Using sun-like cogwheel for Clear sky
        case 1:
        case 2:
        case 3: return LV_SYMBOL_LIST;     // Stylized clouds
        case 45:
        case 48: return LV_SYMBOL_MINUS;    // Horizontal lines for Fog
        case 51: case 53: case 55:
        case 61: case 63: case 65:
        case 80: case 81: case 82: return LV_SYMBOL_DOWNLOAD; // Downward arrow/drop for Rain
        case 71: case 73: case 75:
        case 85: case 86: return LV_SYMBOL_REFRESH;  // Swirly flake for Snow
        case 95: case 96: case 99: return LV_SYMBOL_CHARGE;   // Lightning bolt for Thunderstorm
        default: return LV_SYMBOL_WARNING;  // Unknown
    }
}