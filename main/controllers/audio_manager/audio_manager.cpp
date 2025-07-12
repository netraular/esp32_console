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
#include "freertos/queue.h"
#include <math.h> 
#include "freertos/semphr.h"

static const char *TAG = "AUDIO_MGR";

// --- NUEVO: Constantes para el filtro de paso alto ---
// El volumen físico (0-100) por encima del cual se activará el filtro.
#define HIGH_PASS_FILTER_THRESHOLD 15 
// Coeficiente del filtro (alpha). Un valor más cercano a 1.0 corta más graves.
// Este valor (0.93) corresponde a una frecuencia de corte de aprox. 180Hz para un sample rate de 16kHz.
// Es un buen punto de partida para eliminar los subgraves problemáticos.
#define HIGH_PASS_FILTER_ALPHA 0.93f

// WAV file header structure
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

// Player state variables
static TaskHandle_t playback_task_handle = NULL;
static volatile audio_player_state_t player_state = AUDIO_STATE_STOPPED;
static char current_filepath[256];
static i2s_chan_handle_t tx_chan;
static wav_header_t wav_file_info;
static volatile uint32_t total_bytes_played = 0;
static volatile uint32_t song_duration_s = 0;
static QueueHandle_t visualizer_queue = NULL;

// Volume control variables
// The increment/decrement step for the user-facing volume display (e.g., 0-100%).
#define VOLUME_STEP 5
static volatile uint8_t current_volume_percentage = 5;
static volatile float volume_factor = 0.1f;
static SemaphoreHandle_t volume_mutex = NULL; 

// Task synchronization
static SemaphoreHandle_t playback_task_terminated_sem = NULL;

// Function Prototypes
static void audio_playback_task(void *arg);
static void audio_manager_set_volume_internal(uint8_t percentage, bool apply_cap);


// --- Internal Volume Control ---
static void audio_manager_set_volume_internal(uint8_t percentage, bool apply_cap) {
    if (volume_mutex && xSemaphoreTake(volume_mutex, portMAX_DELAY) == pdTRUE) {
        uint8_t old_percentage = current_volume_percentage;

        if (apply_cap && percentage > MAX_VOLUME_PERCENTAGE) {
            percentage = MAX_VOLUME_PERCENTAGE;
        } else if (percentage > 100) {
            percentage = 100;
        }
        
        current_volume_percentage = percentage;
        volume_factor = (float)current_volume_percentage / 100.0f; 
        
        ESP_LOGI(TAG, "Volume changed: %u%% -> %u%% (physical), factor: %.2f", old_percentage, current_volume_percentage, volume_factor);

        xSemaphoreGive(volume_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take volume mutex in set_volume!");
    }
}


// --- Public Functions ---

void audio_manager_init(void) {
    volume_mutex = xSemaphoreCreateMutex();
    if (volume_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create volume mutex!");
    }

    playback_task_terminated_sem = xSemaphoreCreateBinary();
    if (playback_task_terminated_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create playback termination semaphore!");
    } else {
        // Initialize the semaphore as "given" so the first play call can take it.
        xSemaphoreGive(playback_task_terminated_sem);
    }

    ESP_LOGI(TAG, "Audio Manager Initialized.");
    player_state = AUDIO_STATE_STOPPED;
    audio_manager_set_volume_internal(current_volume_percentage, true);

    visualizer_queue = xQueueCreate(1, sizeof(visualizer_data_t));
    if (visualizer_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create visualizer queue");
    }
}

bool audio_manager_play(const char *filepath) {
    ESP_LOGI(TAG, "Play command received for: %s", filepath);
    if (player_state != AUDIO_STATE_STOPPED) {
        ESP_LOGI(TAG, "Player is not stopped, stopping it first.");
        audio_manager_stop();
    }

    // Wait for the previous task to confirm termination before starting a new one.
    if (xSemaphoreTake(playback_task_terminated_sem, pdMS_TO_TICKS(100)) == pdFALSE) {
        ESP_LOGE(TAG, "Could not start new playback, previous task has not terminated yet.");
        return false;
    }

    strncpy(current_filepath, filepath, sizeof(current_filepath) - 1);
    total_bytes_played = 0;
    song_duration_s = 0;
    player_state = AUDIO_STATE_PLAYING;
    BaseType_t result = xTaskCreate(audio_playback_task, "audio_playback", 4096, NULL, 5, &playback_task_handle);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio playback task");
        player_state = AUDIO_STATE_STOPPED;
        xSemaphoreGive(playback_task_terminated_sem); // Release the semaphore on failure.
        return false;
    }
    ESP_LOGI(TAG, "Audio playback task created. Handle: %p", playback_task_handle);
    return true;
}

void audio_manager_stop(void) {
    ESP_LOGI(TAG, "Stop command received. Current state: %d", player_state);
    
    audio_player_state_t previous_state = player_state;

    if (previous_state != AUDIO_STATE_STOPPED) {
        player_state = AUDIO_STATE_STOPPED;

        // If paused, the playback task might be blocked on i2s_channel_write.
        // Re-enabling the channel ensures the task can proceed to its cleanup logic.
        if (previous_state == AUDIO_STATE_PAUSED && tx_chan) {
            ESP_LOGI(TAG, "Re-enabling I2S channel to unblock task...");
            i2s_channel_enable(tx_chan);
        }

        if (playback_task_handle != NULL) {
             ESP_LOGI(TAG, "Waiting for task (%p) to confirm termination...", playback_task_handle);

             if (xSemaphoreTake(playback_task_terminated_sem, pdMS_TO_TICKS(1000)) == pdTRUE) {
                 ESP_LOGI(TAG, "Task termination confirmed.");
             } else {
                 ESP_LOGW(TAG, "Timed out waiting for playback task to terminate!");
             }
             // Always give the semaphore back so the system can recover.
             xSemaphoreGive(playback_task_terminated_sem);
             
             playback_task_handle = NULL;
        }
        total_bytes_played = 0;
        song_duration_s = 0;
    }
}

void audio_manager_pause(void) {
    if (player_state == AUDIO_STATE_PLAYING && tx_chan) {
        if (i2s_channel_disable(tx_chan) == ESP_OK) {
            player_state = AUDIO_STATE_PAUSED;
            ESP_LOGI(TAG, "Audio Paused.");
        } else {
            ESP_LOGE(TAG, "Failed to disable I2S channel.");
        }
    }
}

void audio_manager_resume(void) {
    if (player_state == AUDIO_STATE_PAUSED && tx_chan) {
        if (i2s_channel_enable(tx_chan) == ESP_OK) {
            player_state = AUDIO_STATE_PLAYING;
            ESP_LOGI(TAG, "Audio Resumed.");
        } else {
            ESP_LOGE(TAG, "Failed to enable I2S channel.");
        }
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

void audio_manager_volume_up(void) {
    uint8_t current_physical_vol = audio_manager_get_volume();

    // 1. Get the current volume mapped to a 0-100 display scale.
    uint8_t raw_display_vol = (current_physical_vol * 100) / MAX_VOLUME_PERCENTAGE;
    // 2. Snap it to the nearest lower step (e.g., 23% -> 20%).
    uint8_t current_snapped_vol = (uint8_t)(roundf((float)raw_display_vol / VOLUME_STEP) * VOLUME_STEP);
    // 3. Add a step and clamp to 100.
    uint8_t new_display_vol = current_snapped_vol + VOLUME_STEP;
    if (new_display_vol > 100) {
        new_display_vol = 100;
    }
    // 4. Convert the new display volume back to the physical hardware volume and apply it.
    uint8_t new_physical_vol = (new_display_vol * MAX_VOLUME_PERCENTAGE + 50) / 100;
    audio_manager_set_volume_internal(new_physical_vol, true);
}

void audio_manager_volume_down(void) {
    uint8_t current_physical_vol = audio_manager_get_volume();
    
    uint8_t raw_display_vol = (current_physical_vol * 100) / MAX_VOLUME_PERCENTAGE;
    uint8_t current_snapped_vol = (uint8_t)(roundf((float)raw_display_vol / VOLUME_STEP) * VOLUME_STEP);
    int temp_display_vol = (int)current_snapped_vol - VOLUME_STEP;
    if (temp_display_vol < 0) {
        temp_display_vol = 0;
    }
    uint8_t new_display_vol = (uint8_t)temp_display_vol;
    uint8_t new_physical_vol = (new_display_vol * MAX_VOLUME_PERCENTAGE + 50) / 100;
    audio_manager_set_volume_internal(new_physical_vol, true);
}

uint8_t audio_manager_get_volume(void) {
    uint8_t vol = 0;
    if (volume_mutex && xSemaphoreTake(volume_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        vol = current_volume_percentage;
        xSemaphoreGive(volume_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take volume mutex in get_volume! Returning potentially stale data.");
        vol = current_volume_percentage;
    }
    return vol;
}

void audio_manager_set_volume_physical(uint8_t percentage) {
    // Set volume directly, do not apply MAX_VOLUME_PERCENTAGE cap
    audio_manager_set_volume_internal(percentage, false);
}

QueueHandle_t audio_manager_get_visualizer_queue(void) {
    return visualizer_queue;
}


// --- Audio Playback Task ---

static void audio_playback_task(void *arg) {
    ESP_LOGI(TAG, "Playback task (%p) started execution.", playback_task_handle);

    FILE *fp = fopen(current_filepath, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s, error: %s (%d)", current_filepath, strerror(errno), errno);
        player_state = AUDIO_STATE_STOPPED;
        playback_task_handle = NULL;
        xSemaphoreGive(playback_task_terminated_sem);
        vTaskDelete(NULL);
        return;
    }

    // This basic WAV parser looks for 'fmt ' and 'data' chunks.
    if (fread(&wav_file_info, 1, 12, fp) != 12) {
        ESP_LOGE(TAG, "Failed to read RIFF header.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; playback_task_handle = NULL; xSemaphoreGive(playback_task_terminated_sem); vTaskDelete(NULL); return;
    }
    if (strncmp(wav_file_info.riff_header, "RIFF", 4) != 0 || strncmp(wav_file_info.wave_header, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Invalid RIFF/WAVE header.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; playback_task_handle = NULL; xSemaphoreGive(playback_task_terminated_sem); vTaskDelete(NULL); return;
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
        fseek(fp, chunk_size, SEEK_CUR); // Skip unknown chunks
    }
    if (!fmt_found) {
        ESP_LOGE(TAG, "'fmt ' chunk not found.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; playback_task_handle = NULL; xSemaphoreGive(playback_task_terminated_sem); vTaskDelete(NULL); return;
    }
    bool data_found = false;
    while(fread(wav_file_info.data_header, 1, 4, fp) == 4 && fread(&wav_file_info.data_size, 1, 4, fp) == 4) {
        if (strncmp(wav_file_info.data_header, "data", 4) == 0) {
            data_found = true;
            break;
        }
        fseek(fp, wav_file_info.data_size, SEEK_CUR); // Skip unknown chunks
    }
    if (!data_found) {
        ESP_LOGE(TAG, "'data' chunk not found.");
        fclose(fp); player_state = AUDIO_STATE_STOPPED; playback_task_handle = NULL; xSemaphoreGive(playback_task_terminated_sem); vTaskDelete(NULL); return;
    }
    ESP_LOGI(TAG, "WAV Info: SR=%lu, BPS=%u, CH=%u, Data Size=%lu", 
             wav_file_info.sample_rate, wav_file_info.bits_per_sample, 
             wav_file_info.num_channels, wav_file_info.data_size);
    if (wav_file_info.byte_rate > 0) {
        song_duration_s = wav_file_info.data_size / wav_file_info.byte_rate;
    }
    
    // --- CAMBIO: Asignación explícita de puerto I2S ---
    // Configure and initialize I2S for speaker output on I2S_NUM_0
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
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
    std_cfg.gpio_cfg.bclk = I2S_SPEAKER_BCLK_PIN;
    std_cfg.gpio_cfg.ws = I2S_SPEAKER_WS_PIN;
    std_cfg.gpio_cfg.dout = I2S_SPEAKER_DOUT_PIN;
    std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv = false;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    int buffer_size = 2048;
    uint8_t *buffer = (uint8_t*)malloc(buffer_size);
    size_t bytes_read, bytes_written;

    // --- NUEVO: Variables de estado para el filtro HPF ---
    // Usamos int32_t para evitar desbordamientos en los cálculos intermedios.
    int32_t last_input_l = 0, last_output_l = 0;
    int32_t last_input_r = 0, last_output_r = 0;


    ESP_LOGI(TAG, "Starting playback... Duration: %lu seconds. Initial Volume: %u%% (physical)", song_duration_s, audio_manager_get_volume());
    total_bytes_played = 0;
    
    // Main playback loop
    while (total_bytes_played < wav_file_info.data_size) {
        if (player_state == AUDIO_STATE_STOPPED) {
            break;
        }
        
        if (player_state == AUDIO_STATE_PAUSED) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        bytes_read = fread(buffer, 1, buffer_size, fp);
        if (bytes_read == 0) {
            if (feof(fp)) {
                ESP_LOGI(TAG, "End of file reached.");
            } else if (ferror(fp)) {
                ESP_LOGE(TAG, "A read error occurred on the file stream. error: %s (%d)", strerror(errno), errno);
                player_state = AUDIO_STATE_ERROR;
            }
            break;
        }

        // --- INICIO DE LA LÓGICA DEL FILTRO Y VOLUMEN ---

        // Obtener el volumen físico actual UNA VEZ por cada bloque de audio
        uint8_t current_physical_vol = audio_manager_get_volume();

        // Aplicar filtro de paso alto si el volumen supera el umbral
        if (current_physical_vol > HIGH_PASS_FILTER_THRESHOLD && wav_file_info.bits_per_sample == 16) {
            int16_t *samples = (int16_t *)buffer;
            size_t num_samples = bytes_read / sizeof(int16_t);

            if (wav_file_info.num_channels == 1) { // MONO
                for (size_t i = 0; i < num_samples; i++) {
                    int32_t current_input = samples[i];
                    int32_t current_output = HIGH_PASS_FILTER_ALPHA * (last_output_l + current_input - last_input_l);
                    
                    last_input_l = current_input;
                    last_output_l = current_output;
                    
                    samples[i] = (int16_t)current_output;
                }
            } else { // STEREO
                for (size_t i = 0; i < num_samples; i += 2) {
                    // Canal Izquierdo (Left)
                    int32_t current_input_l = samples[i];
                    int32_t current_output_l = HIGH_PASS_FILTER_ALPHA * (last_output_l + current_input_l - last_input_l);
                    last_input_l = current_input_l;
                    last_output_l = current_output_l;
                    samples[i] = (int16_t)current_output_l;

                    // Canal Derecho (Right)
                    int32_t current_input_r = samples[i + 1];
                    int32_t current_output_r = HIGH_PASS_FILTER_ALPHA * (last_output_r + current_input_r - last_input_r);
                    last_input_r = current_input_r;
                    last_output_r = current_output_r;
                    samples[i + 1] = (int16_t)current_output_r;
                }
            }
        }
        
        // Process audio data for the visualizer (se hace sobre el audio ya filtrado)
        if (visualizer_queue != NULL && wav_file_info.bits_per_sample == 16) {
            visualizer_data_t viz_data;
            int16_t* samples = (int16_t*)buffer;
            size_t num_samples = bytes_read / sizeof(int16_t);
            size_t samples_per_bar = num_samples / VISUALIZER_BAR_COUNT;

            if (samples_per_bar > 0) {
                for (int i = 0; i < VISUALIZER_BAR_COUNT; i++) {
                    int32_t peak = 0;
                    size_t start_sample = i * samples_per_bar;
                    size_t end_sample = start_sample + samples_per_bar;
                    for (size_t j = start_sample; j < end_sample; j++) {
                        int16_t val = abs(samples[j]);
                        if (val > peak) peak = val;
                    }
                    if (peak > 0) {
                        float log_val = log10f((float)peak);
                        const float sensitivity_divisor = 6.0f;
                        float calculated_height = (log_val / sensitivity_divisor) * 255.0f;
                        
                        viz_data.bar_values[i] = (calculated_height > 255.0f) ? 255 :
                                                 (calculated_height < 0.0f)   ? 0   :
                                                 (uint8_t)calculated_height;
                    } else {
                        viz_data.bar_values[i] = 0;
                    }
                }
                xQueueOverwrite(visualizer_queue, &viz_data);
            }
        }
        
        // Aplicar control de volumen (factor)
        float local_volume_factor = 0.0f;
        if (volume_mutex && xSemaphoreTake(volume_mutex, portMAX_DELAY) == pdTRUE) {
            local_volume_factor = volume_factor;
            xSemaphoreGive(volume_mutex);
        } else {
            ESP_LOGE(TAG, "Failed to take volume mutex in playback task! Using fallback factor 0.0.");
        }

        if (local_volume_factor < 0.999f) {
            if (wav_file_info.bits_per_sample == 16) {
                int16_t *samples = (int16_t *)buffer;
                size_t num_samples = bytes_read / sizeof(int16_t);
                for (size_t i = 0; i < num_samples; i++) {
                    samples[i] = (int16_t)((float)samples[i] * local_volume_factor);
                }
            } else if (wav_file_info.bits_per_sample == 8) {
                uint8_t *samples = (uint8_t *)buffer;
                for (size_t i = 0; i < bytes_read; i++) {
                    int16_t sample_signed = (int16_t)samples[i] - 128;
                    sample_signed = (int16_t)((float)sample_signed * local_volume_factor);
                    if (sample_signed > 127) sample_signed = 127;
                    else if (sample_signed < -128) sample_signed = -128;
                    samples[i] = (uint8_t)(sample_signed + 128);
                }
            }
        }
        
        // --- FIN DE LA LÓGICA DEL FILTRO Y VOLUMEN ---

        i2s_channel_write(tx_chan, buffer, bytes_read, &bytes_written, portMAX_DELAY);
        total_bytes_played += bytes_written;
    }

    // --- Cleanup ---
    ESP_LOGI(TAG, "Playback task (%p) entering cleanup.", playback_task_handle);
    free(buffer);
    fclose(fp);
    if(tx_chan) {
        i2s_channel_disable(tx_chan);
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
    }
    
    if (player_state != AUDIO_STATE_ERROR) {
        player_state = AUDIO_STATE_STOPPED;
    }
    playback_task_handle = NULL;
    
    // Signal that this task has finished and cleaned up.
    xSemaphoreGive(playback_task_terminated_sem);

    ESP_LOGI(TAG, "Playback task self-deleting.");
    vTaskDelete(NULL);
}