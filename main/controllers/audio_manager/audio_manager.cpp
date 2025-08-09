#include "audio_manager.h"
#include "config/board_config.h"
#include "config/app_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "freertos/queue.h"
#include <math.h> 
#include "freertos/semphr.h"

static const char *TAG = "AUDIO_MGR";

// --- CONFIGURACIÓN DEL FILTRO DE PASO ALTO (HPF) LINKWITZ-RILEY DE 4º ORDEN ---
#define HIGH_PASS_FILTER_THRESHOLD 13 
#define HPF_MIN_CUTOFF_FREQ 60.0f   
#define HPF_MAX_CUTOFF_FREQ 350.0f 

#define HPF_LR4_Q1 0.541196f
#define HPF_LR4_Q2 1.306563f

typedef struct { float b0, b1, b2, a1, a2; } biquad_coeffs_t;
typedef struct { float x1, x2, y1, y2; } biquad_state_t;

typedef struct { biquad_state_t left; biquad_state_t right; } stereo_biquad_stage_t;

#define HPF_STAGES 2
typedef struct { stereo_biquad_stage_t stages[HPF_STAGES]; } fourth_order_hpf_state_t;
typedef struct { biquad_coeffs_t stage[HPF_STAGES]; } fourth_order_hpf_coeffs_t;

// Estructura parcial para el header del WAV. La llenaremos por partes.
typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_size;
} wav_format_info_t;

// Player state variables
static TaskHandle_t playback_task_handle = NULL;
static volatile audio_player_state_t player_state = AUDIO_STATE_STOPPED;
static char current_filepath[256];
static i2s_chan_handle_t tx_chan;
static wav_format_info_t wav_file_info; // Usamos la nueva estructura parcial
static volatile uint32_t total_bytes_played = 0;
static volatile uint32_t song_duration_s = 0;
static QueueHandle_t visualizer_queue = NULL;

// Volume control variables
#define VOLUME_STEP 5
static volatile uint8_t current_volume_percentage = 5;
static volatile float volume_factor = 0.1f;
static SemaphoreHandle_t volume_mutex = NULL; 

// Task synchronization
static SemaphoreHandle_t playback_task_terminated_sem = NULL;

// --- VARIABLES GLOBALES PARA EL FILTRO DE 4º ORDEN ---
static fourth_order_hpf_state_t hpf_state;
static fourth_order_hpf_coeffs_t hpf_coeffs;
static volatile bool hpf_active = false;
static uint8_t last_known_volume_for_hpf = 0;

// Function Prototypes
static void audio_playback_task(void *arg);
static void audio_manager_set_volume_internal(uint8_t percentage, bool apply_cap);
static void calculate_lr4_hpf_coeffs(float cutoff_freq, float sample_rate);
static void reset_hpf_state();
static int16_t apply_biquad_filter(biquad_state_t* state, const biquad_coeffs_t* coeffs, int16_t input);
static inline float map_range(float value, float from_low, float from_high, float to_low, float to_high);

// --- FUNCIONES DEL FILTRO BIQUAD ---
static inline float map_range(float value, float from_low, float from_high, float to_low, float to_high) {
    if (value <= from_low) return to_low;
    if (value >= from_high) return to_high;
    return to_low + (to_high - to_low) * ((value - from_low) / (from_high - from_low));
}

static void calculate_lr4_hpf_coeffs(float cutoff_freq, float sample_rate) {
    const float q_values[HPF_STAGES] = { HPF_LR4_Q1, HPF_LR4_Q2 };
    
    for (int i = 0; i < HPF_STAGES; i++) {
        const float q = q_values[i];
        const float omega = 2.0f * M_PI * cutoff_freq / sample_rate;
        const float cos_omega = cosf(omega);
        const float alpha = sinf(omega) / (2.0f * q);
        const float a0_inv = 1.0f / (1.0f + alpha); 

        hpf_coeffs.stage[i].b0 = ((1.0f + cos_omega) / 2.0f) * a0_inv;
        hpf_coeffs.stage[i].b1 = (-(1.0f + cos_omega)) * a0_inv;
        hpf_coeffs.stage[i].b2 = ((1.0f + cos_omega) / 2.0f) * a0_inv;
        hpf_coeffs.stage[i].a1 = (-2.0f * cos_omega) * a0_inv;
        hpf_coeffs.stage[i].a2 = (1.0f - alpha) * a0_inv;
    }
    ESP_LOGD(TAG, "LR4 HPF coeffs calculated for %.1f Hz.", cutoff_freq);
}

static void reset_hpf_state() {
    memset(&hpf_state, 0, sizeof(fourth_order_hpf_state_t));
}

static int16_t apply_biquad_filter(biquad_state_t* state, const biquad_coeffs_t* coeffs, int16_t input) {
    float result = coeffs->b0 * input + coeffs->b1 * state->x1 + coeffs->b2 * state->x2 - coeffs->a1 * state->y1 - coeffs->a2 * state->y2;
    state->x2 = state->x1; state->x1 = input;
    state->y2 = state->y1; state->y1 = result;
    
    if (result > 32767.0f) result = 32767.0f;
    if (result < -32768.0f) result = -32768.0f;
    
    return (int16_t)result;
}


// --- Internal Volume Control ---
static void audio_manager_set_volume_internal(uint8_t percentage, bool apply_cap) {
    if (volume_mutex && xSemaphoreTake(volume_mutex, portMAX_DELAY) == pdTRUE) {
        if (apply_cap && percentage > MAX_VOLUME_PERCENTAGE) percentage = MAX_VOLUME_PERCENTAGE;
        else if (percentage > 100) percentage = 100;
        current_volume_percentage = percentage;
        volume_factor = (float)current_volume_percentage / 100.0f; 
        ESP_LOGI(TAG, "Volume set to %u%% (physical), factor: %.2f", current_volume_percentage, volume_factor);
        xSemaphoreGive(volume_mutex);
    }
}

// --- Public Functions ---
void audio_manager_init(void) {
    volume_mutex = xSemaphoreCreateMutex();
    playback_task_terminated_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(playback_task_terminated_sem);
    audio_manager_set_volume_internal(5, true);
    visualizer_queue = xQueueCreate(1, sizeof(visualizer_data_t));
    ESP_LOGI(TAG, "Audio Manager Initialized.");
}

bool audio_manager_play(const char *filepath) {
    if (player_state != AUDIO_STATE_STOPPED) audio_manager_stop();
    if (xSemaphoreTake(playback_task_terminated_sem, pdMS_TO_TICKS(100)) == pdFALSE) {
        ESP_LOGE(TAG, "Could not start new playback, previous task has not terminated yet.");
        return false;
    }
    strncpy(current_filepath, filepath, sizeof(current_filepath) - 1);
    player_state = AUDIO_STATE_PLAYING;
    if (xTaskCreate(audio_playback_task, "audio_playback", 4096, NULL, 5, &playback_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio playback task");
        player_state = AUDIO_STATE_STOPPED;
        xSemaphoreGive(playback_task_terminated_sem);
        return false;
    }
    return true;
}

void audio_manager_stop(void) {
    if (player_state != AUDIO_STATE_STOPPED) {
        audio_player_state_t prev_state = player_state;
        player_state = AUDIO_STATE_STOPPED;
        if (prev_state == AUDIO_STATE_PAUSED && tx_chan) i2s_channel_enable(tx_chan);
        if (playback_task_handle) {
            if (xSemaphoreTake(playback_task_terminated_sem, pdMS_TO_TICKS(1000)) == pdFALSE) {
                 ESP_LOGW(TAG, "Timed out waiting for playback task to terminate!");
            }
            xSemaphoreGive(playback_task_terminated_sem);
            playback_task_handle = NULL;
        }
        total_bytes_played = 0; 
        song_duration_s = 0; 
    }
}

void audio_manager_pause(void) { if (player_state == AUDIO_STATE_PLAYING && tx_chan) { i2s_channel_disable(tx_chan); player_state = AUDIO_STATE_PAUSED; } }
void audio_manager_resume(void) { if (player_state == AUDIO_STATE_PAUSED && tx_chan) { i2s_channel_enable(tx_chan); player_state = AUDIO_STATE_PLAYING; } }

audio_player_state_t audio_manager_get_state(void) { return player_state; }
uint32_t audio_manager_get_duration_s(void) { return song_duration_s; }
uint32_t audio_manager_get_progress_s(void) { return (wav_file_info.byte_rate > 0) ? (total_bytes_played / wav_file_info.byte_rate) : 0; }

void audio_manager_volume_up(void) {
    uint8_t current_physical_vol = audio_manager_get_volume();
    uint8_t display_vol = (uint8_t)(roundf((float)((current_physical_vol * 100) / MAX_VOLUME_PERCENTAGE) / VOLUME_STEP) * VOLUME_STEP);
    uint8_t next_display_vol = display_vol + VOLUME_STEP;
    if (next_display_vol > 100) next_display_vol = 100;
    uint8_t new_physical_vol = (next_display_vol * MAX_VOLUME_PERCENTAGE + 50) / 100;
    audio_manager_set_volume_internal(new_physical_vol, true);
}

void audio_manager_volume_down(void) {
    uint8_t current_physical_vol = audio_manager_get_volume();
    uint8_t display_vol = (uint8_t)(roundf((float)((current_physical_vol * 100) / MAX_VOLUME_PERCENTAGE) / VOLUME_STEP) * VOLUME_STEP);
    int temp_display_vol = (int)display_vol - VOLUME_STEP;
    if (temp_display_vol < 0) temp_display_vol = 0;
    uint8_t new_physical_vol = ((uint8_t)temp_display_vol * MAX_VOLUME_PERCENTAGE + 50) / 100;
    audio_manager_set_volume_internal(new_physical_vol, true);
}

uint8_t audio_manager_get_volume(void) {
    uint8_t vol = 0;
    if (volume_mutex && xSemaphoreTake(volume_mutex, pdMS_TO_TICKS(100)) == pdTRUE) { vol = current_volume_percentage; xSemaphoreGive(volume_mutex); }
    return vol;
}

void audio_manager_set_volume_physical(uint8_t percentage) { audio_manager_set_volume_internal(percentage, false); }
QueueHandle_t audio_manager_get_visualizer_queue(void) { return visualizer_queue; }

// --- Audio Playback Task ---
static void audio_playback_task(void *arg) {
    ESP_LOGI(TAG, "Playback task started.");
    
    FILE *fp = NULL;
    uint8_t *buffer = NULL;
    i2s_chan_config_t chan_cfg;
    i2s_std_config_t std_cfg;
    const int buffer_size = 2048;

    bool fmt_found = false;
    bool data_found = false;

    fp = fopen(current_filepath, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open file: %s", current_filepath);
        player_state = AUDIO_STATE_ERROR;
        goto cleanup;
    }

    memset(&wav_file_info, 0, sizeof(wav_format_info_t));

    char riff_header[4];
    uint32_t file_size;
    char wave_header[4];
    
    if (fread(riff_header, 1, 4, fp) != 4 || strncmp(riff_header, "RIFF", 4) != 0 ||
        fread(&file_size, 1, 4, fp) != 4 ||
        fread(wave_header, 1, 4, fp) != 4 || strncmp(wave_header, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Invalid RIFF/WAVE header.");
        player_state = AUDIO_STATE_ERROR;
        goto cleanup;
    }
    
    while (!data_found) {
        char chunk_id[4];
        uint32_t chunk_size;

        if (fread(chunk_id, 1, 4, fp) != 4 || fread(&chunk_size, 1, 4, fp) != 4) {
            ESP_LOGE(TAG, "Reached end of file or read error while searching for chunks.");
            player_state = AUDIO_STATE_ERROR;
            goto cleanup;
        }

        if (strncmp(chunk_id, "fmt ", 4) == 0) {
            ESP_LOGD(TAG, "Found 'fmt ' chunk, size: %lu", chunk_size);
            if (chunk_size < 16) {
                ESP_LOGE(TAG, "Invalid 'fmt ' chunk size: %lu", chunk_size);
                player_state = AUDIO_STATE_ERROR;
                goto cleanup;
            }
            fread(&wav_file_info, 1, 16, fp);
            
            if (chunk_size > 16) {
                fseek(fp, chunk_size - 16, SEEK_CUR);
            }
            fmt_found = true;
        } else if (strncmp(chunk_id, "data", 4) == 0) {
            ESP_LOGD(TAG, "Found 'data' chunk, size: %lu", chunk_size);
            wav_file_info.data_size = chunk_size;
            data_found = true;
        } else {
            char id_str[5] = {0};
            strncpy(id_str, chunk_id, 4);
            ESP_LOGI(TAG, "Skipping unknown chunk '%s' of size %lu", id_str, chunk_size);
            fseek(fp, chunk_size, SEEK_CUR);
        }
    }

    if (!fmt_found || !data_found) {
        ESP_LOGE(TAG, "Could not find essential 'fmt ' and 'data' chunks.");
        player_state = AUDIO_STATE_ERROR;
        goto cleanup;
    }

    ESP_LOGI(TAG, "WAV Info: SR=%lu, BPS=%u, CH=%u, Data Size=%lu", 
             wav_file_info.sample_rate, wav_file_info.bits_per_sample, 
             wav_file_info.num_channels, wav_file_info.data_size);
    if (wav_file_info.byte_rate > 0) {
        song_duration_s = wav_file_info.data_size / wav_file_info.byte_rate;
    } else {
        ESP_LOGE(TAG, "Byte rate is zero, cannot calculate duration.");
        player_state = AUDIO_STATE_ERROR;
        goto cleanup;
    }

    chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    // --- CORRECCIÓN: Restaurar la configuración detallada de I2S ---
    std_cfg = {};
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
    std_cfg.gpio_cfg.bclk = I2S_SPEAKER_BCLK_PIN; 
    std_cfg.gpio_cfg.ws = I2S_SPEAKER_WS_PIN;
    std_cfg.gpio_cfg.dout = I2S_SPEAKER_DOUT_PIN; 
    std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false};
    // --- FIN DE LA CORRECCIÓN ---

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    buffer = (uint8_t*)malloc(buffer_size);
    size_t bytes_read, bytes_written;
    total_bytes_played = 0;

    reset_hpf_state();
    last_known_volume_for_hpf = 0;
    
    ESP_LOGI(TAG, "Starting playback... Duration: %lu s", song_duration_s);
    
    while (total_bytes_played < wav_file_info.data_size && player_state != AUDIO_STATE_STOPPED) {
        if (player_state == AUDIO_STATE_PAUSED) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        
        bytes_read = fread(buffer, 1, buffer_size, fp);
        if (bytes_read == 0) {
            if (ferror(fp)) {
                ESP_LOGE(TAG, "File read error: %s", strerror(errno));
                player_state = AUDIO_STATE_ERROR;
            }
            break;
        }

        uint8_t current_physical_vol = audio_manager_get_volume();
        hpf_active = (current_physical_vol >= HIGH_PASS_FILTER_THRESHOLD && wav_file_info.bits_per_sample == 16);

        if (hpf_active) {
            if (current_physical_vol != last_known_volume_for_hpf) {
                float cutoff = map_range(current_physical_vol, HIGH_PASS_FILTER_THRESHOLD, MAX_VOLUME_PERCENTAGE, HPF_MIN_CUTOFF_FREQ, HPF_MAX_CUTOFF_FREQ);
                calculate_lr4_hpf_coeffs(cutoff, (float)wav_file_info.sample_rate);
                last_known_volume_for_hpf = current_physical_vol;
            }
            int16_t *samples = (int16_t *)buffer;
            size_t num_samples = bytes_read / sizeof(int16_t);

            if (wav_file_info.num_channels == 1) {
                for (size_t i = 0; i < num_samples; i++) {
                    int16_t stage1_out = apply_biquad_filter(&hpf_state.stages[0].left, &hpf_coeffs.stage[0], samples[i]);
                    samples[i] = apply_biquad_filter(&hpf_state.stages[1].left, &hpf_coeffs.stage[1], stage1_out);
                }
            } else { 
                for (size_t i = 0; i < num_samples; i += 2) {
                    int16_t stage1_out_l = apply_biquad_filter(&hpf_state.stages[0].left, &hpf_coeffs.stage[0], samples[i]);
                    samples[i] = apply_biquad_filter(&hpf_state.stages[1].left, &hpf_coeffs.stage[1], stage1_out_l);
                    
                    int16_t stage1_out_r = apply_biquad_filter(&hpf_state.stages[0].right, &hpf_coeffs.stage[0], samples[i+1]);
                    samples[i+1] = apply_biquad_filter(&hpf_state.stages[1].right, &hpf_coeffs.stage[1], stage1_out_r);
                }
            }
        }
        
        if (visualizer_queue != NULL && wav_file_info.bits_per_sample == 16) {
            visualizer_data_t viz_data;
            int16_t* samples = (int16_t*)buffer;
            size_t num_samples = bytes_read / sizeof(int16_t);
            size_t samples_per_bar = num_samples / VISUALIZER_BAR_COUNT;
            if (samples_per_bar > 0) {
                for (int i = 0; i < VISUALIZER_BAR_COUNT; i++) {
                    int32_t peak = 0;
                    for (size_t j = i * samples_per_bar; j < (i + 1) * samples_per_bar; j++) {
                        int16_t val = abs(samples[j]);
                        if (val > peak) peak = val;
                    }
                    float log_val = log10f((float)peak + 1.0f);
                    float calculated_height = (log_val / 4.5f) * 255.0f;
                    viz_data.bar_values[i] = (calculated_height > 255.0f) ? 255 : (uint8_t)calculated_height;
                }
                xQueueOverwrite(visualizer_queue, &viz_data);
            }
        }
        
        float local_volume_factor = 0.0f;
        if (volume_mutex && xSemaphoreTake(volume_mutex, portMAX_DELAY) == pdTRUE) {
            local_volume_factor = volume_factor;
            xSemaphoreGive(volume_mutex);
        }

        if (local_volume_factor < 0.999f) {
            if (wav_file_info.bits_per_sample == 16) {
                int16_t *samples = (int16_t *)buffer;
                for (size_t i = 0; i < bytes_read / 2; i++) samples[i] = (int16_t)((float)samples[i] * local_volume_factor);
            } else if (wav_file_info.bits_per_sample == 8) {
                for (size_t i = 0; i < bytes_read; i++) buffer[i] = (uint8_t)((float)(buffer[i] - 128) * local_volume_factor) + 128;
            }
        }
        
        i2s_channel_write(tx_chan, buffer, bytes_read, &bytes_written, portMAX_DELAY);
        if (bytes_written < bytes_read) {
             ESP_LOGW(TAG, "I2S buffer full. Wrote %d of %d bytes.", bytes_written, bytes_read);
        }
        total_bytes_played += bytes_written;
    }

cleanup:
    ESP_LOGI(TAG, "Playback task entering cleanup.");
    if (buffer) free(buffer);
    if (fp) fclose(fp);
    if (tx_chan) {
        i2s_channel_disable(tx_chan);
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
    }
    
    if (player_state != AUDIO_STATE_ERROR) player_state = AUDIO_STATE_STOPPED;
    playback_task_handle = NULL;
    xSemaphoreGive(playback_task_terminated_sem);
    ESP_LOGI(TAG, "Playback task self-deleting.");
    vTaskDelete(NULL);
}