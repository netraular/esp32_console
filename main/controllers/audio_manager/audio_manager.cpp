#include "audio_manager.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <errno.h>

static const char *TAG = "AUDIO_MGR";

// --- Estructuras y variables de estado ---
typedef struct {
    // RIFF Header
    char riff_header[4]; // Contains "RIFF"
    uint32_t wav_size;      // Size of the wav portion of the file
    char wave_header[4]; // Contains "WAVE"
    // Format Header
    char fmt_header[4];  // Contains "fmt " (with space)
    uint32_t fmt_chunk_size; // Should be 16 for PCM
    uint16_t audio_format; // Should be 1 for PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;     // Number of bytes per second
    uint16_t block_align;
    uint16_t bits_per_sample;
    // Data Header
    char data_header[4]; // Contains "data"
    uint32_t data_size;  // Number of bytes in data
} wav_header_t;

static TaskHandle_t playback_task_handle = NULL;
static volatile audio_player_state_t player_state = AUDIO_STATE_STOPPED;
static char current_filepath[256];
static i2s_chan_handle_t tx_chan;

// --- NUEVO NOMBRE DE VARIABLE ---
static wav_header_t wav_file_info; // Almacenamos la cabecera completa
static volatile uint32_t total_bytes_played = 0;
static volatile uint32_t song_duration_s = 0;


// --- Prototipo de la tarea de reproducción ---
static void audio_playback_task(void *arg);

// --- Implementación de funciones públicas ---

void audio_manager_init(void) {
    ESP_LOGI(TAG, "Audio Manager Initialized.");
    player_state = AUDIO_STATE_STOPPED;
}

bool audio_manager_play(const char *filepath) {
    if (player_state != AUDIO_STATE_STOPPED) {
        audio_manager_stop();
    }
    
    strncpy(current_filepath, filepath, sizeof(current_filepath) - 1);

    // Reiniciar contadores
    total_bytes_played = 0;
    song_duration_s = 0;
    
    player_state = AUDIO_STATE_PLAYING;
    
    BaseType_t result = xTaskCreate(audio_playback_task, "audio_playback", 4096, NULL, 5, &playback_task_handle);

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio playback task");
        player_state = AUDIO_STATE_STOPPED;
        return false;
    }

    return true;
}

void audio_manager_stop(void) {
    if (player_state != AUDIO_STATE_STOPPED) {
        ESP_LOGI(TAG, "Stopping audio playback...");
        player_state = AUDIO_STATE_STOPPED;
        if (playback_task_handle != NULL) {
             vTaskDelay(pdMS_TO_TICKS(150)); 
             playback_task_handle = NULL;
        }
        total_bytes_played = 0;
        song_duration_s = 0;
    }
}

void audio_manager_pause(void) {
    if (player_state == AUDIO_STATE_PLAYING) {
        player_state = AUDIO_STATE_PAUSED;
        ESP_LOGI(TAG, "Audio Paused.");
    }
}

void audio_manager_resume(void) {
    if (player_state == AUDIO_STATE_PAUSED) {
        player_state = AUDIO_STATE_PLAYING;
        ESP_LOGI(TAG, "Audio Resumed.");
    }
}

audio_player_state_t audio_manager_get_state(void) {
    return player_state;
}

uint32_t audio_manager_get_duration_s(void) {
    return song_duration_s;
}

uint32_t audio_manager_get_progress_s(void) {
    if (wav_file_info.byte_rate > 0) {
        return total_bytes_played / wav_file_info.byte_rate;
    }
    return 0;
}


// --- Tarea de Reproducción de Audio (LÓGICA CORREGIDA) ---

static void audio_playback_task(void *arg) {
    FILE *fp = fopen(current_filepath, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s, errno: %s", current_filepath, strerror(errno));
        player_state = AUDIO_STATE_STOPPED;
        vTaskDelete(NULL);
        return;
    }

    // --- LÓGICA DE PARSEO DE WAV ROBUSTA ---
    // 1. Leer la cabecera RIFF inicial
    if (fread(&wav_file_info, 1, 12, fp) != 12) {
        ESP_LOGE(TAG, "Failed to read RIFF header.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; vTaskDelete(NULL); return;
    }
    if (strncmp(wav_file_info.riff_header, "RIFF", 4) != 0 || strncmp(wav_file_info.wave_header, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Invalid RIFF/WAVE header.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; vTaskDelete(NULL); return;
    }

    // 2. Buscar el chunk "fmt "
    char chunk_id[4];
    uint32_t chunk_size;
    bool fmt_found = false;
    while(fread(chunk_id, 1, 4, fp) == 4 && fread(&chunk_size, 1, 4, fp) == 4) {
        if (strncmp(chunk_id, "fmt ", 4) == 0) {
            fread(&wav_file_info.audio_format, 1, chunk_size, fp);
            fmt_found = true;
            break;
        }
        fseek(fp, chunk_size, SEEK_CUR); // Saltar este chunk
    }
    if (!fmt_found) {
        ESP_LOGE(TAG, "'fmt ' chunk not found.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; vTaskDelete(NULL); return;
    }
    
    // 3. Buscar el chunk "data"
    bool data_found = false;
    while(fread(wav_file_info.data_header, 1, 4, fp) == 4 && fread(&wav_file_info.data_size, 1, 4, fp) == 4) {
        if (strncmp(wav_file_info.data_header, "data", 4) == 0) {
            data_found = true;
            break;
        }
        fseek(fp, wav_file_info.data_size, SEEK_CUR); // Saltar este chunk
    }
    if (!data_found) {
        ESP_LOGE(TAG, "'data' chunk not found.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; vTaskDelete(NULL); return;
    }
    // --- FIN LÓGICA DE PARSEO ---

    ESP_LOGI(TAG, "WAV Info: SR=%lu, BPS=%u, CH=%u, Data Size=%lu", 
             wav_file_info.sample_rate, wav_file_info.bits_per_sample, 
             wav_file_info.num_channels, wav_file_info.data_size);
    
    // Calcular y almacenar duración
    if (wav_file_info.byte_rate > 0) {
        song_duration_s = wav_file_info.data_size / wav_file_info.byte_rate;
    }
    
    // --- Configuración I2S ---
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(wav_file_info.sample_rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t)wav_file_info.bits_per_sample, 
                                                    (wav_file_info.num_channels == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_DOUT_PIN,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    // --- Bucle de reproducción ---
    int buffer_size = 2048;
    uint8_t *buffer = (uint8_t*)malloc(buffer_size);
    size_t bytes_read, bytes_written;

    ESP_LOGI(TAG, "Starting playback... Duration: %lu seconds.", song_duration_s);
    total_bytes_played = 0;

    while (player_state != AUDIO_STATE_STOPPED && total_bytes_played < wav_file_info.data_size) {
        while (player_state == AUDIO_STATE_PAUSED) {
            vTaskDelay(pdMS_TO_TICKS(100));
            if (player_state == AUDIO_STATE_STOPPED) goto cleanup;
        }

        bytes_read = fread(buffer, 1, buffer_size, fp);
        if (bytes_read == 0) break; // Fin de archivo

        i2s_channel_write(tx_chan, buffer, bytes_read, &bytes_written, portMAX_DELAY);
        total_bytes_played += bytes_written;
    }

cleanup:
    ESP_LOGI(TAG, "Playback finished or stopped. Cleaning up.");
    free(buffer);
    fclose(fp);
    if(tx_chan) {
        i2s_channel_disable(tx_chan);
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
    }
    
    player_state = AUDIO_STATE_STOPPED;
    playback_task_handle = NULL;
    vTaskDelete(NULL);
}