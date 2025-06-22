#ifndef CONFIG_H
#define CONFIG_H

#include <driver/gpio.h>

// --- DISPLAY SPI BUS (SPI2_HOST) ---
#define SPI_SCLK_PIN    GPIO_NUM_14
#define SPI_MOSI_PIN    GPIO_NUM_13
#define SPI_MISO_PIN    GPIO_NUM_NC

// --- DEDICATED SD CARD SPI BUS (SPI3_HOST) ---
#define SD_SPI_SCLK_PIN   GPIO_NUM_47
#define SD_SPI_MOSI_PIN   GPIO_NUM_21
#define SD_SPI_MISO_PIN   GPIO_NUM_2
#define SD_CS_PIN         GPIO_NUM_1

// --- DISPLAY PINS ---
#define TFT_RST   GPIO_NUM_12
#define TFT_DC    GPIO_NUM_11
#define TFT_CS    GPIO_NUM_10
#define TFT_BL    GPIO_NUM_9

// --- BUTTON PINS ---
#define BUTTON_ON_OFF_PIN  GPIO_NUM_8
#define BUTTON_LEFT_PIN    GPIO_NUM_7
#define BUTTON_CANCEL_PIN  GPIO_NUM_6
#define BUTTON_OK_PIN      GPIO_NUM_5
#define BUTTON_RIGHT_PIN   GPIO_NUM_4

// --- LED PINS ---
#define RGB_LED_PIN      GPIO_NUM_48

// --- AUDIO PINS ---
// #define BUZZER_PIN   GPIO_NUM_18

// Unified I2S Pins for Audio In/Out
#define I2S_BCLK_PIN   GPIO_NUM_16
#define I2S_WS_PIN     GPIO_NUM_17
#define I2S_DOUT_PIN   GPIO_NUM_15 // To Amplifier/Speaker
#define I2S_DIN_PIN    GPIO_NUM_38 // From Microphone inmp441

// --- DISPLAY & SPI CONFIGURATION ---
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240
#define LCD_HOST      SPI2_HOST
#define SD_HOST       SPI3_HOST
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define LVGL_TICK_PERIOD_MS 10

// --- AUDIO CONFIGURATION ---
// Sets a safety limit on the physical volume (0-100) to protect the speaker.
// The UI will still show 0-100%, but it will be mapped to this physical range.
#define MAX_VOLUME_PERCENTAGE 25 

// --- RECORDING CONFIGURATION ---
#define REC_SAMPLE_RATE 16000
#define REC_BITS_PER_SAMPLE 16
#define REC_NUM_CHANNELS 1

#endif // CONFIG_H