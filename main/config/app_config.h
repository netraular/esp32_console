#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "driver/spi_common.h" // For SPI2_HOST, SPI3_HOST

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
#define MAX_VOLUME_PERCENTAGE 50 // For a small speaker, 25 is a good value

// --- RECORDING CONFIGURATION ---
#define REC_SAMPLE_RATE 16000
#define REC_BITS_PER_SAMPLE 16
#define REC_NUM_CHANNELS 1

// --- BUTTON CONFIGURATION ---
// Time in milliseconds to wait for a second click. If exceeded, a SINGLE_CLICK is registered.
#define BUTTON_DOUBLE_CLICK_MS      300
// Time in milliseconds to hold a button to trigger a LONG_PRESS event.
#define BUTTON_LONG_PRESS_MS        1000

// --- API & NETWORK CONFIGURATION ---
#define WEATHER_API_URL "https://api.open-meteo.com/v1/forecast?latitude=41.39&longitude=2.16&hourly=weather_code&forecast_days=2&timezone=Europe%2FBerlin"
#define WEATHER_FETCH_INTERVAL_MS (30 * 60 * 1000) // 30 minutes

#endif // APP_CONFIG_H