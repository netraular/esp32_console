#include "stt_manager.h"
#include "config/secrets.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include <memory>
#include <vector>

static const char* TAG = "STT_MANAGER";
#define GROQ_TRANSCRIPTIONS_URL "https://api.groq.com/openai/v1/audio/transcriptions"
#define STT_MODEL "whisper-large-v3-turbo"
#define BOUNDARY "----WebKitFormBoundary7MA4YWxkTrZu0gW"
#define HTTP_POST_BUFFER_SIZE 2048

extern const char groq_api_ca_pem_start[] asm("_binary_groq_api_ca_pem_start");

// A helper class to encapsulate the state of a single transcription request.
// This makes the code much cleaner and safer than passing a raw C struct.
class SttRequestContext {
public:
    SttRequestContext(const std::string& path, stt_result_callback_t func)
        : file_path(path), callback(func) {}

    std::string file_path;
    stt_result_callback_t callback;
    std::string response_buffer;
};

// The HTTP event handler now appends data to a std::string, which is much safer.
static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    auto* context = static_cast<SttRequestContext*>(evt->user_data);
    if (!context) {
        return ESP_FAIL;
    }

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_DATA: {
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            
            // Get current size and calculate required new capacity
            size_t current_size = context->response_buffer.length();
            size_t required_capacity = current_size + evt->data_len;

            // Pre-allocate memory without using exceptions
            context->response_buffer.reserve(required_capacity);

            // Check if reservation was successful by comparing capacity
            if (context->response_buffer.capacity() < required_capacity) {
                ESP_LOGE(TAG, "Failed to reserve memory for response buffer!");
                return ESP_FAIL; // Signal failure
            }

            // Append data now that we know there is space
            context->response_buffer.append(static_cast<const char*>(evt->data), evt->data_len);
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}


static void stt_transcription_task(void *pvParameters) {
    // Take ownership of the context object. It will be automatically deleted when the task exits.
    std::unique_ptr<SttRequestContext> context(static_cast<SttRequestContext*>(pvParameters));

    esp_http_client_handle_t client = nullptr;
    FILE* audio_file = nullptr;
    std::string result_text;
    bool success = false;
    
    do {
        ESP_LOGI(TAG, "STT task started. Waiting for WiFi & Time Sync...");
        EventGroupHandle_t wifi_event_group = wifi_manager_get_event_group();
        if (!wifi_event_group) {
            ESP_LOGE(TAG, "WiFi event group not available!");
            result_text = "Error: WiFi infrastructure not ready.";
            break;
        }

        EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | TIME_SYNC_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(20000));
        if ((bits & (WIFI_CONNECTED_BIT | TIME_SYNC_BIT)) != (WIFI_CONNECTED_BIT | TIME_SYNC_BIT)) {
            ESP_LOGE(TAG, "Timed out waiting for WiFi connection and time sync.");
            result_text = "Error: WiFi/Time not ready.";
            break;
        }
        ESP_LOGI(TAG, "WiFi & Time Sync are ready. Proceeding with transcription.");
        
        audio_file = fopen(context->file_path.c_str(), "rb");
        if (!audio_file) {
            ESP_LOGE(TAG, "Failed to open audio file: %s", context->file_path.c_str());
            result_text = "Error: Could not open audio file.";
            break;
        }

        fseek(audio_file, 0, SEEK_END);
        long file_size = ftell(audio_file);
        fseek(audio_file, 0, SEEK_SET);

        std::string body_header = 
            "--" BOUNDARY "\r\n"
            "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
            STT_MODEL "\r\n"
            "--" BOUNDARY "\r\n"
            "Content-Disposition: form-data; name=\"response_format\"\r\n\r\n"
            "json\r\n"
            "--" BOUNDARY "\r\n"
            "Content-Disposition: form-data; name=\"file\"; filename=\"note.wav\"\r\n"
            "Content-Type: audio/wav\r\n\r\n";

        std::string body_footer = "\r\n--" BOUNDARY "--\r\n";
        
        long total_content_length = body_header.length() + file_size + body_footer.length();
        
        esp_http_client_config_t config = {};
        config.url = GROQ_TRANSCRIPTIONS_URL;
        config.event_handler = _http_event_handler;
        config.user_data = context.get(); // Pass the context pointer
        config.method = HTTP_METHOD_POST;
        config.timeout_ms = 30000;
        config.cert_pem = groq_api_ca_pem_start;
        config.buffer_size = 2048;

        client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to initialize HTTP client");
            result_text = "Error: HTTP client init failed.";
            break;
        }

        std::string auth_header = "Bearer " + std::string(GROQ_API_KEY);
        esp_http_client_set_header(client, "Authorization", auth_header.c_str());

        std::string content_type_header = "multipart/form-data; boundary=" + std::string(BOUNDARY);
        esp_http_client_set_header(client, "Content-Type", content_type_header.c_str());

        if (esp_http_client_open(client, total_content_length) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection.");
            result_text = "Error: HTTP connection failed.";
            break;
        }
        
        if (esp_http_client_write(client, body_header.c_str(), body_header.length()) < 0) {
             ESP_LOGE(TAG, "Failed to write multipart headers");
             result_text = "Error: HTTP header write failed.";
             break;
        }

        std::vector<char> file_read_buffer(HTTP_POST_BUFFER_SIZE);
        size_t bytes_read;
        while ((bytes_read = fread(file_read_buffer.data(), 1, file_read_buffer.size(), audio_file)) > 0) {
            if (esp_http_client_write(client, file_read_buffer.data(), bytes_read) < 0) {
                ESP_LOGE(TAG, "Failed to write HTTP data");
                result_text = "Error: HTTP data send failed.";
                break;
            }
        }
        if (!result_text.empty()) break; // Exit if an error occurred in the loop

        if (esp_http_client_write(client, body_footer.c_str(), body_footer.length()) < 0) {
             ESP_LOGE(TAG, "Failed to write final boundary");
             result_text = "Error: HTTP final boundary write failed.";
             break;
        }

        // Fetch headers first to get the final response length
        if (esp_http_client_fetch_headers(client) < 0) {
             ESP_LOGE(TAG, "HTTP client fetch headers failed");
             result_text = "Error: HTTP fetch headers failed.";
             break;
        }
        
        // Now perform the read loop
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Status = %d", status_code);

        // The response body is already being collected by the event handler
        esp_err_t read_err = esp_http_client_read_response(client, nullptr, 0);
        if (read_err != ESP_OK && read_err != ESP_ERR_HTTP_EAGAIN) {
             ESP_LOGE(TAG, "HTTP client read response failed: %s", esp_err_to_name(read_err));
        }
        
        if (status_code == 200) {
            cJSON *root = cJSON_Parse(context->response_buffer.c_str());
            if (root) {
                cJSON *text_item = cJSON_GetObjectItem(root, "text");
                if (cJSON_IsString(text_item) && (text_item->valuestring != NULL)) {
                    result_text = text_item->valuestring;
                    success = true;
                } else {
                    result_text = "Error: 'text' field not found in JSON response.";
                }
                cJSON_Delete(root);
            } else {
                result_text = "Error: Failed to parse JSON response.";
            }
        } else {
            result_text = "Error: HTTP " + std::to_string(status_code) + " - " + (context->response_buffer.empty() ? "No details" : context->response_buffer);
        }

    } while(0);

    // --- Automatic-style Cleanup ---
    if (audio_file) fclose(audio_file);
    if (client) esp_http_client_cleanup(client);
    
    // --- Safe Callback Invocation ---
    if (context->callback) {
        if (!success && result_text.empty()) {
            result_text = "Unknown transcription error";
        }
        context->callback(success, result_text);
    }
    
    ESP_LOGI(TAG, "STT task finished.");
    vTaskDelete(NULL);
}

void stt_manager_init(void) {
    ESP_LOGI(TAG, "STT Manager Initialized.");
}

bool stt_manager_transcribe(const std::string& file_path, stt_result_callback_t cb) {
    if (file_path.empty() || !cb) {
        ESP_LOGE(TAG, "Invalid arguments for transcription.");
        return false;
    }

    auto* context = new(std::nothrow) SttRequestContext(file_path, cb);
    if (!context) {
        ESP_LOGE(TAG, "Failed to allocate memory for task context");
        return false;
    }

    BaseType_t result = xTaskCreate(stt_transcription_task, "stt_task", 8192, context, 5, NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create STT transcription task");
        delete context; 
        return false;
    }

    return true;
}