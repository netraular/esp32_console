#include "stt_manager.h"
#include "secret.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include "controllers/wifi_manager/wifi_manager.h"

static const char* TAG = "STT_MANAGER";
#define GROQ_TRANSCRIPTIONS_URL "https://api.groq.com/openai/v1/audio/transcriptions"
#define STT_MODEL "whisper-large-v3-turbo"
#define BOUNDARY "----WebKitFormBoundary7MA4YWxkTrZu0gW"
#define HTTP_POST_BUFFER_SIZE 2048

extern const char groq_api_ca_pem_start[] asm("_binary_groq_api_ca_pem_start");
extern const char groq_api_ca_pem_end[]   asm("_binary_groq_api_ca_pem_end");

typedef struct {
    char* file_path;
    stt_result_callback_t callback;
    char* response_buffer;
    int response_len;
} stt_task_params_t;

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    stt_task_params_t* params = (stt_task_params_t*)evt->user_data;
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (params->response_buffer == NULL) {
                params->response_buffer = (char*) malloc(evt->data_len + 1);
            } else {
                params->response_buffer = (char*) realloc(params->response_buffer, params->response_len + evt->data_len + 1);
            }

            if (params->response_buffer) {
                memcpy(params->response_buffer + params->response_len, evt->data, evt->data_len);
                params->response_len += evt->data_len;
                params->response_buffer[params->response_len] = '\0';
            } else {
                 ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void stt_transcription_task(void *pvParameters) {
    stt_task_params_t *params = (stt_task_params_t *)pvParameters;
    
    esp_http_client_handle_t client = NULL;
    FILE* audio_file = NULL;
    char* file_read_buffer = NULL;
    char* result_text = NULL;
    bool success = false;
    long file_size = 0;
    
    do {
        ESP_LOGI(TAG, "STT task started. Waiting for WiFi & Time Sync...");
        EventGroupHandle_t wifi_event_group = wifi_manager_get_event_group();
        if (!wifi_event_group) {
            ESP_LOGE(TAG, "WiFi event group not available!");
            asprintf(&result_text, "Error: WiFi event group not available.");
            break;
        }

        EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                               WIFI_CONNECTED_BIT | TIME_SYNC_BIT,
                                               pdFALSE,
                                               pdTRUE,
                                               pdMS_TO_TICKS(20000));

        if ((bits & (WIFI_CONNECTED_BIT | TIME_SYNC_BIT)) != (WIFI_CONNECTED_BIT | TIME_SYNC_BIT)) {
            ESP_LOGE(TAG, "Timed out waiting for WiFi connection and time sync.");
            asprintf(&result_text, "Error: WiFi/Time not ready.");
            break;
        }
        ESP_LOGI(TAG, "WiFi & Time Sync are ready. Proceeding with transcription.");
        
        audio_file = fopen(params->file_path, "rb");
        if (!audio_file) {
            ESP_LOGE(TAG, "Failed to open audio file: %s", params->file_path);
            asprintf(&result_text, "Error: Could not open audio file.");
            break;
        }

        fseek(audio_file, 0, SEEK_END);
        file_size = ftell(audio_file);
        fseek(audio_file, 0, SEEK_SET);

        char body_header[512];
        int header_len = snprintf(body_header, sizeof(body_header), 
                                  "--%s\r\n"
                                  "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
                                  "%s\r\n"
                                  "--%s\r\n"
                                  "Content-Disposition: form-data; name=\"response_format\"\r\n\r\n"
                                  "json\r\n"
                                  "--%s\r\n"
                                  "Content-Disposition: form-data; name=\"file\"; filename=\"note.wav\"\r\n"
                                  "Content-Type: audio/wav\r\n\r\n", 
                                  BOUNDARY, STT_MODEL, BOUNDARY, BOUNDARY);

        char body_footer[128];
        int footer_len = snprintf(body_footer, sizeof(body_footer), 
                                  "\r\n--%s--\r\n", 
                                  BOUNDARY);
        
        long total_content_length = header_len + file_size + footer_len;
        
        esp_http_client_config_t config = {};
        config.url = GROQ_TRANSCRIPTIONS_URL;
        config.event_handler = _http_event_handler;
        config.user_data = params;
        config.method = HTTP_METHOD_POST;
        config.timeout_ms = 30000;
        config.cert_pem = groq_api_ca_pem_start;
        // --- CORRECCIÓN FINAL: Aumentar el tamaño del búfer de recepción ---
        config.buffer_size = 2048; // Aumentar de 512 (por defecto) a 2KB

        client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to initialize HTTP client");
            asprintf(&result_text, "Error: HTTP client init failed.");
            break;
        }

        char auth_header[128];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", GROQ_API_KEY);
        esp_http_client_set_header(client, "Authorization", auth_header);

        char content_type_header[128];
        snprintf(content_type_header, sizeof(content_type_header), "multipart/form-data; boundary=%s", BOUNDARY);
        esp_http_client_set_header(client, "Content-Type", content_type_header);

        esp_err_t err = esp_http_client_open(client, total_content_length);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            asprintf(&result_text, "Error: HTTP connection failed.");
            break;
        }
        
        if(esp_http_client_write(client, body_header, header_len) < 0) {
             ESP_LOGE(TAG, "Failed to write multipart headers");
             asprintf(&result_text, "Error: HTTP header write failed.");
             break;
        }

        file_read_buffer = (char*) malloc(HTTP_POST_BUFFER_SIZE);
        if (!file_read_buffer) {
             ESP_LOGE(TAG, "Failed to allocate file read buffer");
             asprintf(&result_text, "Error: Memory for file buffer failed.");
             break;
        }
        size_t bytes_read;
        while ((bytes_read = fread(file_read_buffer, 1, HTTP_POST_BUFFER_SIZE, audio_file)) > 0) {
            if (esp_http_client_write(client, file_read_buffer, bytes_read) < 0) {
                ESP_LOGE(TAG, "Failed to write HTTP data");
                asprintf(&result_text, "Error: HTTP data send failed.");
                goto stream_error;
            }
        }
        free(file_read_buffer);
        file_read_buffer = NULL;

        if(esp_http_client_write(client, body_footer, footer_len) < 0) {
             ESP_LOGE(TAG, "Failed to write final boundary");
             asprintf(&result_text, "Error: HTTP final boundary write failed.");
        }
        
    stream_error:
        if (result_text) break;

        // Ahora, en lugar de llamar a fetch_headers, que lee solo las cabeceras,
        // llamamos a perform, que se encarga de todo el proceso de recibir el cuerpo.
        err = esp_http_client_perform(client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "HTTP client perform failed: %s", esp_err_to_name(err));
            asprintf(&result_text, "Error: HTTP perform failed.");
            break;
        }
        
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Status = %d", status_code);
        
        if (params->response_buffer) {
            ESP_LOGI(TAG, "Raw Response Body:\n%.*s", params->response_len, params->response_buffer);
        }

        if (status_code == 200) {
            cJSON *root = cJSON_Parse(params->response_buffer);
            if (root) {
                cJSON *text_item = cJSON_GetObjectItem(root, "text");
                if (cJSON_IsString(text_item) && (text_item->valuestring != NULL)) {
                    result_text = strdup(text_item->valuestring);
                    success = true;
                } else {
                    result_text = strdup("Error: 'text' field not found in JSON response.");
                }
                cJSON_Delete(root);
            } else {
                result_text = strdup("Error: Failed to parse JSON response.");
            }
        } else {
            asprintf(&result_text, "Error: HTTP %d - %.*s", status_code, params->response_len, params->response_buffer ? params->response_buffer : "No details");
        }

    } while(0);

    if (audio_file) fclose(audio_file);
    if (client) esp_http_client_cleanup(client);
    if (file_read_buffer) free(file_read_buffer);
    if (params->response_buffer) free(params->response_buffer);
    
    if (params->callback) {
        if (!result_text) {
             result_text = strdup("Unknown transcription error");
        }
        params->callback(success, result_text);
    } else {
        if(result_text) free(result_text);
    }
    
    free(params->file_path);
    free(params);
    vTaskDelete(NULL);
}

void stt_manager_init(void) {
    ESP_LOGI(TAG, "STT Manager Initialized.");
}

bool stt_manager_transcribe(const char* file_path, stt_result_callback_t cb) {
    if (!file_path || !cb) {
        ESP_LOGE(TAG, "Invalid arguments for transcription.");
        return false;
    }

    stt_task_params_t* params = (stt_task_params_t*)malloc(sizeof(stt_task_params_t));
    if (!params) {
        ESP_LOGE(TAG, "Failed to allocate memory for task parameters");
        return false;
    }
    memset(params, 0, sizeof(stt_task_params_t));
    params->file_path = strdup(file_path);
    params->callback = cb;

    BaseType_t result = xTaskCreate(stt_transcription_task, "stt_task", 8192, params, 5, NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create STT transcription task");
        free(params->file_path);
        free(params);
        return false;
    }

    return true;
}