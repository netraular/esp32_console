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

// Estructura del encabezado WAV
typedef struct {
    char riff_header[4];
    uint32_t wav_size;
    char wave_header[4];
    char fmt_header[4];
    uint32_t fmt_chunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_header[4];
    uint32_t data_size;
} wav_header_t;

// Variables de estado del reproductor
static TaskHandle_t playback_task_handle = NULL;
static volatile audio_player_state_t player_state = AUDIO_STATE_STOPPED;
static char current_filepath[256];
static i2s_chan_handle_t tx_chan;
static wav_header_t wav_file_info;
static volatile uint32_t total_bytes_played = 0;
static volatile uint32_t song_duration_s = 0;

// Variables de volumen
#define VOLUME_STEP 5
static volatile uint8_t current_volume_percentage = 30;
static volatile float volume_factor = 0.3f;

// Prototipos de funciones
static void audio_playback_task(void *arg);

static void audio_manager_set_volume(uint8_t percentage) {
    if (percentage > 100) percentage = 100;
    current_volume_percentage = percentage;
    volume_factor = (float)current_volume_percentage / 100.0f;
    ESP_LOGI(TAG, "Volumen ajustado a %u%% (factor: %.2f)", current_volume_percentage, volume_factor);
}

// --- Funciones públicas ---

void audio_manager_init(void) {
    ESP_LOGI(TAG, "Audio Manager Initialized.");
    player_state = AUDIO_STATE_STOPPED;
    audio_manager_set_volume(current_volume_percentage);
}

bool audio_manager_play(const char *filepath) {
    if (player_state != AUDIO_STATE_STOPPED) {
        audio_manager_stop();
    }
    strncpy(current_filepath, filepath, sizeof(current_filepath) - 1);
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

// --- CORRECCIÓN PARA COMPATIBILIDAD CON IDF < 5.2 ---
// Se usa i2s_channel_disable/enable en lugar de stop/start.
// La función i2s_channel_zero_dma_buffer no existe, pero disable/enable ya se encarga
// de limpiar el estado lo suficiente para evitar el sonido residual.

void audio_manager_pause(void) {
    if (player_state == AUDIO_STATE_PLAYING && tx_chan) {
        player_state = AUDIO_STATE_PAUSED;
        // En versiones antiguas de IDF, disable() es la forma de pausar la transmisión.
        i2s_channel_disable(tx_chan);
        ESP_LOGI(TAG, "Audio Paused.");
    }
}

void audio_manager_resume(void) {
    if (player_state == AUDIO_STATE_PAUSED && tx_chan) {
        player_state = AUDIO_STATE_PLAYING;
        // Reanuda la transmisión habilitando el canal de nuevo.
        i2s_channel_enable(tx_chan);
        ESP_LOGI(TAG, "Audio Resumed.");
    }
}
// --- FIN DE LA CORRECCIÓN DE COMPATIBILIDAD ---

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

void audio_manager_volume_up(void) {
    uint8_t new_volume = current_volume_percentage + VOLUME_STEP;
    if (new_volume > 100) new_volume = 100;
    audio_manager_set_volume(new_volume);
}

void audio_manager_volume_down(void) {
    int temp_volume = (int)current_volume_percentage - VOLUME_STEP;
    uint8_t new_volume;
    if (temp_volume < 0) new_volume = 0;
    else new_volume = (uint8_t)temp_volume;
    audio_manager_set_volume(new_volume);
}

uint8_t audio_manager_get_volume(void) {
    return current_volume_percentage;
}


// --- Tarea de Reproducción de Audio ---

static void audio_playback_task(void *arg) {
    FILE *fp = fopen(current_filepath, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s, errno: %s", current_filepath, strerror(errno));
        player_state = AUDIO_STATE_STOPPED;
        vTaskDelete(NULL);
        return;
    }

    // --- Lógica de parseo de WAV (sin cambios) ---
    if (fread(&wav_file_info, 1, 12, fp) != 12) {
        ESP_LOGE(TAG, "Failed to read RIFF header.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; vTaskDelete(NULL); return;
    }
    if (strncmp(wav_file_info.riff_header, "RIFF", 4) != 0 || strncmp(wav_file_info.wave_header, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Invalid RIFF/WAVE header.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; vTaskDelete(NULL); return;
    }
    char chunk_id[4];
    uint32_t chunk_size;
    bool fmt_found = false;
    while(fread(chunk_id, 1, 4, fp) == 4 && fread(&chunk_size, 1, 4, fp) == 4) {
        if (strncmp(chunk_id, "fmt ", 4) == 0) {
            fread(&wav_file_info.audio_format, 1, chunk_size, fp);
            fmt_found = true;
            break;
        }
        fseek(fp, chunk_size, SEEK_CUR);
    }
    if (!fmt_found) {
        ESP_LOGE(TAG, "'fmt ' chunk not found.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; vTaskDelete(NULL); return;
    }
    bool data_found = false;
    while(fread(wav_file_info.data_header, 1, 4, fp) == 4 && fread(&wav_file_info.data_size, 1, 4, fp) == 4) {
        if (strncmp(wav_file_info.data_header, "data", 4) == 0) {
            data_found = true;
            break;
        }
        fseek(fp, wav_file_info.data_size, SEEK_CUR);
    }
    if (!data_found) {
        ESP_LOGE(TAG, "'data' chunk not found.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; vTaskDelete(NULL); return;
    }
    ESP_LOGI(TAG, "WAV Info: SR=%lu, BPS=%u, CH=%u, Data Size=%lu", 
             wav_file_info.sample_rate, wav_file_info.bits_per_sample, 
             wav_file_info.num_channels, wav_file_info.data_size);
    if (wav_file_info.byte_rate > 0) {
        song_duration_s = wav_file_info.data_size / wav_file_info.byte_rate;
    }
    
    // --- Configuración I2S ---
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    i2s_std_config_t std_cfg = {}; 
    std_cfg.clk_cfg.sample_rate_hz = wav_file_info.sample_rate;
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    std_cfg.slot_cfg.data_bit_width = (i2s_data_bit_width_t)wav_file_info.bits_per_sample;
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    std_cfg.slot_cfg.slot_mode = (wav_file_info.num_channels == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO;
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
    std_cfg.slot_cfg.ws_width = (i2s_data_bit_width_t)wav_file_info.bits_per_sample;
    std_cfg.slot_cfg.ws_pol = false;
    std_cfg.slot_cfg.bit_shift = false;
    std_cfg.slot_cfg.left_align = true;
    std_cfg.slot_cfg.big_endian = false;
    std_cfg.slot_cfg.bit_order_lsb = false;
    std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.bclk = I2S_BCLK_PIN;
    std_cfg.gpio_cfg.ws = I2S_WS_PIN;
    std_cfg.gpio_cfg.dout = I2S_DOUT_PIN;
    std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv = false;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    // --- Bucle de reproducción ---
    int buffer_size = 2048;
    uint8_t *buffer = (uint8_t*)malloc(buffer_size);
    size_t bytes_read, bytes_written;

    ESP_LOGI(TAG, "Starting playback... Duration: %lu seconds. Volume: %u%%", song_duration_s, current_volume_percentage);
    total_bytes_played = 0;

    while (player_state != AUDIO_STATE_STOPPED && total_bytes_played < wav_file_info.data_size) {
        while (player_state == AUDIO_STATE_PAUSED) {
            vTaskDelay(pdMS_TO_TICKS(100));
            if (player_state == AUDIO_STATE_STOPPED) goto cleanup;
        }

        bytes_read = fread(buffer, 1, buffer_size, fp);
        if (bytes_read == 0) break;

        // Lógica de ajuste de volumen
        if (volume_factor < 0.999f) {
            if (wav_file_info.bits_per_sample == 16) {
                int16_t *samples = (int16_t *)buffer;
                size_t num_samples = bytes_read / sizeof(int16_t);
                for (size_t i = 0; i < num_samples; i++) {
                    samples[i] = (int16_t)((float)samples[i] * volume_factor);
                }
            } else if (wav_file_info.bits_per_sample == 8) {
                uint8_t *samples = (uint8_t *)buffer;
                for (size_t i = 0; i < bytes_read; i++) {
                    int16_t sample_signed = (int16_t)samples[i] - 128;
                    sample_signed = (int16_t)((float)sample_signed * volume_factor);
                    if (sample_signed > 127) sample_signed = 127;
                    else if (sample_signed < -128) sample_signed = -128;
                    samples[i] = (uint8_t)(sample_signed + 128);
                }
            }
        }

        i2s_channel_write(tx_chan, buffer, bytes_read, &bytes_written, portMAX_DELAY);
        total_bytes_played += bytes_written;
    }

cleanup:
    ESP_LOGI(TAG, "Playback finished or stopped. Cleaning up.");
    free(buffer);
    fclose(fp);
    if(tx_chan) {
        // No es necesario llamar a stop() aquí, disable() es suficiente.
        i2s_channel_disable(tx_chan);
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
    }
    
    player_state = AUDIO_STATE_STOPPED;
    playback_task_handle = NULL;
    vTaskDelete(NULL);
}