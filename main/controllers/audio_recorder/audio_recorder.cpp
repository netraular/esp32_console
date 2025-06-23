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
#include <math.h> // For roundf if needed, though direct conversion is likely fine.

static const char *TAG = "AUDIO_REC";

// WAV file header structure (unchanged)
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

// Recorder state variables (unchanged)
static TaskHandle_t recording_task_handle = NULL;
static volatile audio_recorder_state_t recorder_state = RECORDER_STATE_IDLE;
static char current_filepath[256];
static i2s_chan_handle_t rx_chan;
static volatile time_t start_time;

static void audio_recording_task(void *arg);

// --- Public Functions (unchanged) ---
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
    ESP_LOGI(TAG, "Audio recording task created for file: %s (Targeting %d-bit WAV)", filepath, REC_BITS_PER_SAMPLE);
    return true;
}

void audio_recorder_stop(void) {
    if (recorder_state == RECORDER_STATE_RECORDING) {
        ESP_LOGI(TAG, "Stop command received. Signalling task to terminate.");
        recorder_state = RECORDER_STATE_SAVING;
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

// --- WAV Header Creation (unchanged) ---
static void create_wav_header(wav_header_t *header, uint32_t sample_rate, uint16_t bits_per_sample, uint16_t num_channels, uint32_t data_size) {
    memcpy(header->riff_header, "RIFF", 4);
    memcpy(header->wave_header, "WAVE", 4);
    memcpy(header->fmt_header, "fmt ", 4);
    header->fmt_chunk_size = 16;
    header->audio_format = 1;    // PCM
    header->num_channels = num_channels;
    header->sample_rate = sample_rate;
    header->bits_per_sample = bits_per_sample;
    header->byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
    header->block_align = num_channels * (bits_per_sample / 8);
    memcpy(header->data_header, "data", 4);
    header->data_size = data_size;
    header->wav_size = 36 + data_size;

    ESP_LOGD(TAG, "WAV Header: SR=%lu, BPS=%u, CH=%u, ByteRate=%lu, BlockAlign=%u, DataSize=%lu, WavSize=%lu",
             header->sample_rate, header->bits_per_sample, header->num_channels,
             header->byte_rate, header->block_align, header->data_size, header->wav_size);
}

// --- Simplified Audio Recording Task ---
static void audio_recording_task(void *arg) {
    FILE *fp = fopen(current_filepath, "wb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s, error: %s", current_filepath, strerror(errno));
        recorder_state = RECORDER_STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }

    // Write a placeholder WAV header. It will be updated at the end.
    wav_header_t wav_header;
    create_wav_header(&wav_header, REC_SAMPLE_RATE, REC_BITS_PER_SAMPLE, REC_NUM_CHANNELS, 0);
    fwrite(&wav_header, 1, sizeof(wav_header_t), fp);

    // I2S Channel Configuration (RX for microphone)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    // I2S Standard Mode Configuration
    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg.sample_rate_hz = REC_SAMPLE_RATE;
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256; // Or I2S_MCLK_MULTIPLE_DEFAULT
    // Configure for INMP441 (or similar I2S mics) that output 24-bit data in a 32-bit slot
    std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    std_cfg.slot_cfg.slot_mode = (REC_NUM_CHANNELS == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO;
    std_cfg.slot_cfg.slot_mask = (REC_NUM_CHANNELS == 2) ? I2S_STD_SLOT_BOTH : I2S_STD_SLOT_LEFT; // Assuming mono uses left slot
    std_cfg.slot_cfg.ws_width = I2S_SLOT_BIT_WIDTH_32BIT; // Word select width
    std_cfg.slot_cfg.ws_pol = false;                      // WS polarity
    std_cfg.slot_cfg.bit_shift = false;                   // No bit shift for MSB data
    std_cfg.slot_cfg.left_align = true;                  // Data is MSB aligned (left-aligned)
    std_cfg.slot_cfg.big_endian = false;                  // Little-endian
    std_cfg.slot_cfg.bit_order_lsb = false;               // MSB first
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

    // Buffer for reading raw 32-bit data from I2S
    const int i2s_buffer_size_bytes = 2048;
    int32_t* i2s_read_buffer = (int32_t*)malloc(i2s_buffer_size_bytes);
    if (!i2s_read_buffer) {
        ESP_LOGE(TAG, "Failed to allocate I2S read buffer");
        fclose(fp);
        if (rx_chan) { i2s_channel_disable(rx_chan); i2s_del_channel(rx_chan); rx_chan = NULL; }
        recorder_state = RECORDER_STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }

    // Buffer for samples to be written to the WAV file (formatted to REC_BITS_PER_SAMPLE)
    const int bytes_per_sample_wav = REC_BITS_PER_SAMPLE / 8;
    const int num_samples_in_i2s_buffer = i2s_buffer_size_bytes / sizeof(int32_t);
    const int file_write_buffer_size_bytes = num_samples_in_i2s_buffer * bytes_per_sample_wav;
    uint8_t* file_write_buffer = (uint8_t*)malloc(file_write_buffer_size_bytes);
     if (!file_write_buffer) {
        ESP_LOGE(TAG, "Failed to allocate file write buffer");
        free(i2s_read_buffer);
        fclose(fp);
        if (rx_chan) { i2s_channel_disable(rx_chan); i2s_del_channel(rx_chan); rx_chan = NULL; }
        recorder_state = RECORDER_STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }

    size_t bytes_read_from_i2s;
    uint32_t total_data_bytes_written_to_file = 0;

    ESP_LOGI(TAG, "Starting recording loop. Target WAV: %d-bit, %d Hz, %d channel(s).",
             REC_BITS_PER_SAMPLE, REC_SAMPLE_RATE, REC_NUM_CHANNELS);

    while (recorder_state == RECORDER_STATE_RECORDING) {
        esp_err_t result = i2s_channel_read(rx_chan, i2s_read_buffer, i2s_buffer_size_bytes, &bytes_read_from_i2s, pdMS_TO_TICKS(1000));

        if (result == ESP_OK && bytes_read_from_i2s > 0) {
            int samples_processed_from_i2s_buffer = bytes_read_from_i2s / sizeof(int32_t);
            int current_file_buffer_offset = 0;

            for (int i = 0; i < samples_processed_from_i2s_buffer; i++) {
                // Assuming INMP441 or similar: 24-bit data is in the MSBs of the 32-bit slot.
                // Right-shift to get the 24-bit signed value.
                int32_t sample_24bit_signed = i2s_read_buffer[i] >> 8;

                // Clamp to 24-bit range (good practice, though direct data should be in range)
                const int32_t max_24bit_val = (1 << 23) - 1;
                const int32_t min_24bit_val = -(1 << 23);
                if (sample_24bit_signed > max_24bit_val) sample_24bit_signed = max_24bit_val;
                else if (sample_24bit_signed < min_24bit_val) sample_24bit_signed = min_24bit_val;

                if (REC_BITS_PER_SAMPLE == 24) {
                    // Write 3 bytes (Little Endian for WAV)
                    file_write_buffer[current_file_buffer_offset++] = (uint8_t)(sample_24bit_signed & 0xFF);
                    file_write_buffer[current_file_buffer_offset++] = (uint8_t)((sample_24bit_signed >> 8) & 0xFF);
                    file_write_buffer[current_file_buffer_offset++] = (uint8_t)((sample_24bit_signed >> 16) & 0xFF);
                } else if (REC_BITS_PER_SAMPLE == 16) {
                    // Convert 24-bit to 16-bit by taking the MSBs (discard lower 8 bits of 24-bit sample)
                    int16_t sample_16bit = (int16_t)(sample_24bit_signed >> 8);
                    file_write_buffer[current_file_buffer_offset++] = (uint8_t)(sample_16bit & 0xFF);
                    file_write_buffer[current_file_buffer_offset++] = (uint8_t)((sample_16bit >> 8) & 0xFF);
                } else if (REC_BITS_PER_SAMPLE == 8) {
                    // Convert 24-bit to 8-bit unsigned (0-255)
                    // (sample_24bit_signed / 2^16) + 128
                    int8_t sample_8bit_signed = (int8_t)(sample_24bit_signed >> 16); // take MSB of 24bit, results in signed 8bit
                    uint8_t sample_8bit_unsigned = (uint8_t)(sample_8bit_signed + 128);
                    file_write_buffer[current_file_buffer_offset++] = sample_8bit_unsigned;
                }
                // Add other bit depths if needed, e.g., 32-bit float or int.
            }

            if (current_file_buffer_offset > 0) {
                 size_t bytes_written_to_file_this_round;
                 bytes_written_to_file_this_round = fwrite(file_write_buffer, 1, current_file_buffer_offset, fp);
                 if (bytes_written_to_file_this_round < current_file_buffer_offset) {
                    ESP_LOGE(TAG, "File write failed. Wrote %d of %d bytes. Error: %s",
                             bytes_written_to_file_this_round, current_file_buffer_offset, strerror(errno));
                    recorder_state = RECORDER_STATE_ERROR;
                    break;
                 }
                total_data_bytes_written_to_file += bytes_written_to_file_this_round;
            }

        } else if (result == ESP_ERR_TIMEOUT) {
            ESP_LOGV(TAG, "I2S read timeout, continuing...");
            // This is not necessarily an error if no data is coming in for a short period.
        }
        else {
            ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(result));
            recorder_state = RECORDER_STATE_ERROR;
            break;
        }
    }

    ESP_LOGI(TAG, "Recording loop finished. Total data bytes written to file: %lu", total_data_bytes_written_to_file);

    // --- Cleanup ---
    free(i2s_read_buffer);
    free(file_write_buffer);

    if (rx_chan) {
        i2s_channel_disable(rx_chan);
        i2s_del_channel(rx_chan);
        rx_chan = NULL;
    }

    // Update the WAV header with the final data size
    ESP_LOGI(TAG, "Updating WAV header with final data size: %lu", total_data_bytes_written_to_file);
    create_wav_header(&wav_header, REC_SAMPLE_RATE, REC_BITS_PER_SAMPLE, REC_NUM_CHANNELS, total_data_bytes_written_to_file);
    fseek(fp, 0, SEEK_SET); // Go to the beginning of the file
    fwrite(&wav_header, 1, sizeof(wav_header_t), fp); // Write the updated header
    fclose(fp);

    if (recorder_state != RECORDER_STATE_ERROR) {
      recorder_state = RECORDER_STATE_IDLE;
    }

    ESP_LOGI(TAG, "Recording task finished and cleaned up for %s.", current_filepath);
    recording_task_handle = NULL;
    vTaskDelete(NULL);
}