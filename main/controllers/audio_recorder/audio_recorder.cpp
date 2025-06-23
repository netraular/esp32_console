#include "audio_recorder.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "freertos/semphr.h"
#include <time.h>

static const char *TAG = "AUDIO_REC";

// WAV file header structure
typedef struct {
    char     riff_header[4];
    uint32_t wav_size;
    char     wave_header[4];
    char     fmt_header[4];
    uint32_t fmt_chunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data_header[4];
    uint32_t data_size;
} wav_header_t;

// Recorder state variables
static TaskHandle_t recording_task_handle = NULL;
static volatile audio_recorder_state_t recorder_state = RECORDER_STATE_IDLE;
static char current_filepath[256];
static i2s_chan_handle_t rx_chan; // Receive channel for microphone
static volatile time_t start_time;

// Task Prototypes
static void audio_recording_task(void *arg);

// --- Public Functions ---

void audio_recorder_init(void) {
    recorder_state = RECORDER_STATE_IDLE;
    ESP_LOGI(TAG, "Audio Recorder Initialized.");
}

bool audio_recorder_start(const char *filepath) {
    if (recorder_state != RECORDER_STATE_IDLE) {
        ESP_LOGE(TAG, "Recorder is already busy (state: %d)", recorder_state);
        return false;
    }

    strncpy(current_filepath, filepath, sizeof(current_filepath) - 1);
    recorder_state = RECORDER_STATE_RECORDING;
    start_time = time(NULL);

    BaseType_t result = xTaskCreate(audio_recording_task, "audio_record", 4096, NULL, 5, &recording_task_handle);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio recording task");
        recorder_state = RECORDER_STATE_IDLE;
        return false;
    }
    ESP_LOGI(TAG, "Audio recording task created for file: %s", filepath);
    return true;
}

void audio_recorder_stop(void) {
    if (recorder_state == RECORDER_STATE_RECORDING) {
        ESP_LOGI(TAG, "Stop command received. Signalling task to terminate.");
        recorder_state = RECORDER_STATE_SAVING; // Signal task to stop loop and save
    }
}

audio_recorder_state_t audio_recorder_get_state(void) {
    return recorder_state;
}

uint32_t audio_recorder_get_duration_s(void) {
    if (recorder_state == RECORDER_STATE_RECORDING) {
        return (uint32_t)(time(NULL) - start_time);
    }
    return 0;
}

// --- Private Functions ---

static void create_wav_header(wav_header_t *header, uint32_t sample_rate, uint16_t bits_per_sample, uint16_t num_channels, uint32_t data_size) {
    memcpy(header->riff_header, "RIFF", 4);
    header->wav_size = 36 + data_size; // 36 is the size of the header without riff_header and wav_size
    memcpy(header->wave_header, "WAVE", 4);
    memcpy(header->fmt_header, "fmt ", 4);
    header->fmt_chunk_size = 16;
    header->audio_format = 1; // PCM
    header->num_channels = num_channels;
    header->sample_rate = sample_rate;
    header->bits_per_sample = bits_per_sample;
    header->byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
    header->block_align = num_channels * (bits_per_sample / 8);
    memcpy(header->data_header, "data", 4);
    header->data_size = data_size;
}

static void audio_recording_task(void *arg) {
    FILE *fp = fopen(current_filepath, "wb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s, error: %s", current_filepath, strerror(errno));
        recorder_state = RECORDER_STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }

    // Write a placeholder header first
    wav_header_t wav_header;
    create_wav_header(&wav_header, REC_SAMPLE_RATE, REC_BITS_PER_SAMPLE, REC_NUM_CHANNELS, 0);
    fwrite(&wav_header, 1, sizeof(wav_header_t), fp);

    // Configure and initialize I2S for microphone input
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    i2s_std_config_t std_cfg = {}; // Initialize to zeros
    
    // Clock configuration (explicit)
    std_cfg.clk_cfg.sample_rate_hz = REC_SAMPLE_RATE;
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    // Slot configuration
    std_cfg.slot_cfg.data_bit_width = (i2s_data_bit_width_t)REC_BITS_PER_SAMPLE;
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT; 
    
    // GPIO configuration
    std_cfg.gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = I2S_MIC_BCLK_PIN,
        .ws = I2S_MIC_WS_PIN,
        .dout = I2S_GPIO_UNUSED,
        .din = I2S_MIC_DIN_PIN,
        .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    int buffer_size = 2048;
    uint8_t *buffer = (uint8_t*)malloc(buffer_size);
    size_t bytes_read;
    uint32_t total_bytes_written = 0;

    ESP_LOGI(TAG, "Starting recording loop...");

    while (recorder_state == RECORDER_STATE_RECORDING) {
        esp_err_t result = i2s_channel_read(rx_chan, buffer, buffer_size, &bytes_read, pdMS_TO_TICKS(1000));
        if (result == ESP_OK && bytes_read > 0) {
            fwrite(buffer, 1, bytes_read, fp);
            total_bytes_written += bytes_read;
        } else {
            ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(result));
            recorder_state = RECORDER_STATE_ERROR;
            break;
        }
    }

    ESP_LOGI(TAG, "Recording loop finished. Total bytes written: %lu", total_bytes_written);

    // --- Cleanup and Finalize ---
    free(buffer);
    i2s_channel_disable(rx_chan);
    i2s_del_channel(rx_chan);
    rx_chan = NULL;

    // Update the WAV header with the correct file size
    ESP_LOGI(TAG, "Updating WAV header.");
    create_wav_header(&wav_header, REC_SAMPLE_RATE, REC_BITS_PER_SAMPLE, REC_NUM_CHANNELS, total_bytes_written);
    fseek(fp, 0, SEEK_SET);
    fwrite(&wav_header, 1, sizeof(wav_header_t), fp);
    fclose(fp);

    if(recorder_state != RECORDER_STATE_ERROR) {
      recorder_state = RECORDER_STATE_IDLE;
    }
    
    ESP_LOGI(TAG, "Recording task finished and cleaned up.");
    recording_task_handle = NULL;
    vTaskDelete(NULL);
}