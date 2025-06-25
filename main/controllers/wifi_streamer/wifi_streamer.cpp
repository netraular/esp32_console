#include "wifi_streamer.h"
#include "config.h"
#include "secret.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>

static const char *TAG = "WIFI_STREAMER";

// --- I2S Configuration ---
#define I2S_SAMPLE_RATE         (16000)
#define I2S_BUFFER_SAMPLES_READ (1024)
#define I2S_BUFFER_BYTES_READ   (I2S_BUFFER_SAMPLES_READ * sizeof(int32_t))
#define I2S_BUFFER_BYTES_SEND   (I2S_BUFFER_SAMPLES_READ * sizeof(int16_t))

// --- State Variables ---
static TaskHandle_t s_stream_task_handle = NULL;
static volatile wifi_stream_state_t s_streamer_state = WIFI_STREAM_STATE_IDLE;
static char s_status_message[128] = "Idle";

// --- Function Prototypes ---
static void audio_stream_task(void *pvParameters);
static void update_status_message(const char* format, ...);
static esp_err_t setup_i2s_for_streaming(i2s_chan_handle_t *rx_handle);

// --- Public API ---

void wifi_streamer_init(void) {
    s_streamer_state = WIFI_STREAM_STATE_IDLE;
    update_status_message("Idle");
}

bool wifi_streamer_start(void) {
    if (s_stream_task_handle != NULL) {
        ESP_LOGE(TAG, "Streamer task is already running.");
        return false;
    }
    update_status_message("Starting...");
    s_streamer_state = WIFI_STREAM_STATE_CONNECTING;
    BaseType_t result = xTaskCreate(audio_stream_task, "audio_stream_task", 4096, NULL, 5, &s_stream_task_handle);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio stream task");
        update_status_message("Error: Task creation failed");
        s_streamer_state = WIFI_STREAM_STATE_ERROR;
        s_stream_task_handle = NULL;
        return false;
    }
    return true;
}

void wifi_streamer_stop(void) {
    if (s_stream_task_handle != NULL && s_streamer_state < WIFI_STREAM_STATE_STOPPING) {
        ESP_LOGI(TAG, "Signaling stream task to stop.");
        update_status_message("Stopping...");
        s_streamer_state = WIFI_STREAM_STATE_STOPPING;
    }
}

wifi_stream_state_t wifi_streamer_get_state(void) {
    return s_streamer_state;
}

void wifi_streamer_get_status_message(char* buffer, size_t buffer_size) {
    if (buffer && buffer_size > 0) {
        strncpy(buffer, s_status_message, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    }
}

// --- Internal Functions ---

static void update_status_message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(s_status_message, sizeof(s_status_message), format, args);
    va_end(args);
    ESP_LOGI(TAG, "Status: %s", s_status_message);
}

// --- CORRECCIÓN: Configuración de I2S completa y robusta ---
static esp_err_t setup_i2s_for_streaming(i2s_chan_handle_t *rx_handle) {
    // El micrófono está en I2S_NUM_1 para no entrar en conflicto con el altavoz en I2S_NUM_0.
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configuración I2S Standard, basada en la que funciona en audio_recorder.cpp.
    // Es crucial para micrófonos I2S MEMS como el INMP441.
    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg.sample_rate_hz = I2S_SAMPLE_RATE;
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    // Configuración de slot para datos alineados a la izquierda (MSB)
    std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT; // INMP441 es mono, usa el canal izquierdo
    std_cfg.slot_cfg.ws_width = I2S_SLOT_BIT_WIDTH_32BIT;
    std_cfg.slot_cfg.ws_pol = false;
    std_cfg.slot_cfg.bit_shift = false;      // Sin bit shift para datos alineados a MSB
    std_cfg.slot_cfg.left_align = true;      // Datos alineados a la izquierda (MSB)
    
    std_cfg.gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = I2S_MIC_BCLK_PIN,
        .ws   = I2S_MIC_WS_PIN,
        .dout = I2S_GPIO_UNUSED,
        .din  = I2S_MIC_DIN_PIN,
        .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
    };

    ret = i2s_channel_init_std_mode(*rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(ret));
        i2s_del_channel(*rx_handle);
        *rx_handle = NULL;
        return ret;
    }
    
    ret = i2s_channel_enable(*rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        // No es necesario llamar a disable si enable falló, solo borrar el canal
        i2s_del_channel(*rx_handle);
        *rx_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "I2S driver for streaming configured and enabled successfully.");
    return ESP_OK;
}

static void audio_stream_task(void *pvParameters) {
    int sock = -1;
    i2s_chan_handle_t rx_handle = NULL;
    int32_t* i2s_raw_read_buffer = NULL;
    int16_t* i2s_processed_send_buffer = NULL;

    // --- Esperar a WiFi ---
    update_status_message("Waiting for WiFi...");
    while (!wifi_manager_is_connected()) {
        if (s_streamer_state == WIFI_STREAM_STATE_STOPPING) goto cleanup;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // --- Reservar Buffers ---
    i2s_raw_read_buffer = (int32_t*)malloc(I2S_BUFFER_BYTES_READ);
    i2s_processed_send_buffer = (int16_t*)malloc(I2S_BUFFER_BYTES_SEND);
    if (!i2s_raw_read_buffer || !i2s_processed_send_buffer) {
        update_status_message("Error: Memory allocation failed");
        s_streamer_state = WIFI_STREAM_STATE_ERROR;
        goto cleanup;
    }
    
    // --- Configurar I2S ---
    if (setup_i2s_for_streaming(&rx_handle) != ESP_OK) {
        update_status_message("Error: I2S init failed");
        s_streamer_state = WIFI_STREAM_STATE_ERROR;
        goto cleanup;
    }

    // --- Bucle Principal: Conectar y Transmitir ---
    while (s_streamer_state != WIFI_STREAM_STATE_STOPPING) {
        // --- Conexión TCP ---
        update_status_message("Connecting to %s:%d", STREAMING_SERVER_IP, STREAMING_SERVER_PORT);
        s_streamer_state = WIFI_STREAM_STATE_CONNECTING;
        
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(STREAMING_SERVER_IP);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(STREAMING_SERVER_PORT);
        
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            update_status_message("Error: Failed to create socket");
            s_streamer_state = WIFI_STREAM_STATE_ERROR;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue; // Reintentar conexión
        }

        if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
            update_status_message("Error: Connection failed");
            close(sock);
            sock = -1;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue; // Reintentar conexión
        }
        
        update_status_message("Streaming...");
        s_streamer_state = WIFI_STREAM_STATE_STREAMING;
        
        // --- Bucle de Transmisión ---
        while (s_streamer_state == WIFI_STREAM_STATE_STREAMING) {
            size_t bytes_read_from_i2s = 0;
            esp_err_t result = i2s_channel_read(rx_handle, (void*)i2s_raw_read_buffer,
                                                I2S_BUFFER_BYTES_READ, &bytes_read_from_i2s, pdMS_TO_TICKS(100));

            if (result == ESP_ERR_TIMEOUT) {
                continue; // Normal si no hay datos, comprueba la señal de parada
            }
            if (result != ESP_OK) {
                update_status_message("Error: I2S read failed");
                s_streamer_state = WIFI_STREAM_STATE_ERROR;
                break; // Salir del bucle de transmisión
            }

            // Convertir 32-bit a 16-bit. Con la config I2S correcta, esto funciona.
            int num_samples_read = bytes_read_from_i2s / sizeof(int32_t);
            for (int i = 0; i < num_samples_read; i++) {
                // Desplazar para obtener los 16 bits más significativos
                i2s_processed_send_buffer[i] = (int16_t)(i2s_raw_read_buffer[i] >> 16);
            }

            // Enviar datos
            int bytes_to_send = num_samples_read * sizeof(int16_t);
            if (send(sock, i2s_processed_send_buffer, bytes_to_send, 0) < 0) {
                update_status_message("Error: Send failed. Disconnected.");
                s_streamer_state = WIFI_STREAM_STATE_ERROR;
                break; // Salir del bucle de transmisión
            }
        }
        
        // --- Desconectar ---
        if (sock >= 0) {
            close(sock);
            sock = -1;
        }

        // Si salimos del streaming por un error, esperar antes de reintentar
        if (s_streamer_state == WIFI_STREAM_STATE_ERROR) {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

cleanup:
    ESP_LOGI(TAG, "Cleaning up stream task...");
    if (sock >= 0) close(sock);
    if (rx_handle) {
        i2s_channel_disable(rx_handle);
        i2s_del_channel(rx_handle);
    }
    if(i2s_raw_read_buffer) free(i2s_raw_read_buffer);
    if(i2s_processed_send_buffer) free(i2s_processed_send_buffer);
    
    update_status_message("Idle");
    s_streamer_state = WIFI_STREAM_STATE_IDLE;
    s_stream_task_handle = NULL;
    vTaskDelete(NULL);
}