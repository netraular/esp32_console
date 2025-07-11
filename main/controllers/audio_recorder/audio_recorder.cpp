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
#include <math.h> 

static const char *TAG = "AUDIO_REC";

// --- Recording Gain Factor ---
// Due to a potentially weak input signal from the MEMS microphone,
// it may be necessary to apply significant digital gain. A value of 10.0f is a
// reasonable starting point. Adjust if the audio is too quiet or distorted.
#define RECORDING_GAIN (35.0f) 

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
static i2s_chan_handle_t rx_chan = NULL; // Initialize to NULL for robust cleanup
static volatile time_t start_time;

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
    ESP_LOGI(TAG, "Audio recording task created for file: %s (Targeting %d-bit WAV with gain %.2f)", filepath, REC_BITS_PER_SAMPLE, RECORDING_GAIN);
    return true;
}

void audio_recorder_stop(void) {
    if (recorder_state == RECORDER_STATE_RECORDING) {
        ESP_LOGI(TAG, "Stop command received. Signalling task to terminate and save.");
        recorder_state = RECORDER_STATE_SAVING;
    }
}

void audio_recorder_cancel(void) {
    if (recorder_state == RECORDER_STATE_RECORDING) {
        ESP_LOGI(TAG, "Cancel command received. Signalling task to terminate and discard.");
        recorder_state = RECORDER_STATE_CANCELLING;
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

// --- WAV Header Creation ---
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
}

// --- Audio Recording Task with Robust Error Handling ---
static void audio_recording_task(void *arg) {
    FILE *fp = NULL;
    int32_t* i2s_raw_read_buffer = NULL;
    int16_t* file_write_buffer = NULL;
    uint32_t total_data_bytes_written_to_file = 0;

    // This loop runs only once and allows using 'break' as a structured 'goto' for cleanup.
    do {
        fp = fopen(current_filepath, "wb");
        if (fp == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing: %s, error: %s", current_filepath, strerror(errno));
            recorder_state = RECORDER_STATE_ERROR;
            break; // Jump to cleanup
        }

        wav_header_t wav_header;
        create_wav_header(&wav_header, REC_SAMPLE_RATE, REC_BITS_PER_SAMPLE, REC_NUM_CHANNELS, 0);
        fwrite(&wav_header, 1, sizeof(wav_header_t), fp);

        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

        i2s_std_config_t std_cfg = {
            .clk_cfg = {
                .sample_rate_hz = REC_SAMPLE_RATE,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .ext_clk_freq_hz = 0,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, (REC_NUM_CHANNELS == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO),
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = I2S_MIC_BCLK_PIN,
                .ws   = I2S_MIC_WS_PIN,
                .dout = I2S_GPIO_UNUSED,
                .din  = I2S_MIC_DIN_PIN,
                .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
            },
        };
        std_cfg.slot_cfg.slot_mask = (REC_NUM_CHANNELS == 2) ? I2S_STD_SLOT_BOTH : I2S_STD_SLOT_LEFT;

        ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
        ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

        const int i2s_buffer_size_bytes = 4096;
        i2s_raw_read_buffer = (int32_t*)malloc(i2s_buffer_size_bytes);
        if (!i2s_raw_read_buffer) {
            ESP_LOGE(TAG, "Failed to allocate I2S read buffer");
            recorder_state = RECORDER_STATE_ERROR;
            break; // Jump to cleanup
        }

        const int num_samples_in_buffer = i2s_buffer_size_bytes / sizeof(int32_t);
        const int file_write_buffer_size_bytes = num_samples_in_buffer * (REC_BITS_PER_SAMPLE / 8);
        file_write_buffer = (int16_t*)malloc(file_write_buffer_size_bytes);
        if (!file_write_buffer) {
            ESP_LOGE(TAG, "Failed to allocate file write buffer");
            recorder_state = RECORDER_STATE_ERROR;
            break; // Jump to cleanup
        }

        ESP_LOGI(TAG, "Starting recording loop...");
        while (recorder_state == RECORDER_STATE_RECORDING) {
            size_t bytes_read_from_i2s;
            esp_err_t result = i2s_channel_read(rx_chan, i2s_raw_read_buffer, i2s_buffer_size_bytes, &bytes_read_from_i2s, pdMS_TO_TICKS(1000));
            
            if (result == ESP_OK && bytes_read_from_i2s > 0) {
                if (REC_BITS_PER_SAMPLE == 16) {
                    int samples_read = bytes_read_from_i2s / sizeof(int32_t);
                    for (int i = 0; i < samples_read; i++) {
                        int32_t sample_32bit = i2s_raw_read_buffer[i];
                        int16_t original_sample = (int16_t)(sample_32bit >> 16);
                        int32_t amplified_sample = (int32_t)((float)original_sample * RECORDING_GAIN);
                        if (amplified_sample > 32767) amplified_sample = 32767;
                        else if (amplified_sample < -32768) amplified_sample = -32768;
                        file_write_buffer[i] = (int16_t)amplified_sample;
                    }

                    size_t bytes_to_write_to_file = samples_read * sizeof(int16_t);
                    size_t bytes_written = fwrite(file_write_buffer, 1, bytes_to_write_to_file, fp);
                    if (bytes_written < bytes_to_write_to_file) {
                        ESP_LOGE(TAG, "File write failed. Wrote %d of %d bytes", (int)bytes_written, (int)bytes_to_write_to_file);
                        recorder_state = RECORDER_STATE_ERROR;
                        break; 
                    }
                    total_data_bytes_written_to_file += bytes_written;
                } else {
                    ESP_LOGE(TAG, "Unsupported REC_BITS_PER_SAMPLE. Gain/Conversion only for 16-bit.");
                    recorder_state = RECORDER_STATE_ERROR;
                    break;
                }
            } else if (result != ESP_ERR_TIMEOUT) {
                ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(result));
                recorder_state = RECORDER_STATE_ERROR;
                break;
            }
        } // End of while (recording)

    } while(0); // The loop runs only once.

    // --- Centralized Cleanup Block ---
    audio_recorder_state_t final_state = recorder_state;
    ESP_LOGI(TAG, "Recording task stopping. Reason: State changed to %d", final_state);

    if (i2s_raw_read_buffer) free(i2s_raw_read_buffer);
    if (file_write_buffer) free(file_write_buffer);
    if (rx_chan) {
        i2s_channel_disable(rx_chan);
        i2s_del_channel(rx_chan);
        rx_chan = NULL;
    }

    if (fp) {
        if (final_state == RECORDER_STATE_SAVING && total_data_bytes_written_to_file > 0) {
            ESP_LOGI(TAG, "Finalizing WAV file. Updating header with final data size: %lu", total_data_bytes_written_to_file);
            fseek(fp, 0, SEEK_SET);
            wav_header_t final_header;
            create_wav_header(&final_header, REC_SAMPLE_RATE, REC_BITS_PER_SAMPLE, REC_NUM_CHANNELS, total_data_bytes_written_to_file);
            fwrite(&final_header, 1, sizeof(wav_header_t), fp);
        }
        fclose(fp);

        if (final_state == RECORDER_STATE_CANCELLING || final_state == RECORDER_STATE_ERROR) {
            ESP_LOGI(TAG, "Recording cancelled or errored. Deleting file: %s", current_filepath);
            if (unlink(current_filepath) != 0) {
                ESP_LOGE(TAG, "Failed to delete temporary file %s. Error: %s", current_filepath, strerror(errno));
            }
        }
    }

    if (final_state != RECORDER_STATE_ERROR) {
      recorder_state = RECORDER_STATE_IDLE;
    }

    ESP_LOGI(TAG, "Recording task finished and cleaned up for %s.", current_filepath);
    recording_task_handle = NULL;
    vTaskDelete(NULL); // The task self-deletes
}