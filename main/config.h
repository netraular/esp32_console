#ifndef CONFIG_H
#define CONFIG_H

#include <driver/gpio.h>

// --- BUS SPI PANTALLA (SPI2_HOST) ---
// ... (pines sin cambios)
#define SPI_SCLK_PIN    GPIO_NUM_14
#define SPI_MOSI_PIN    GPIO_NUM_13
#define SPI_MISO_PIN    GPIO_NUM_NC

// --- BUS SPI TARJETA SD (NUEVO, DEDICADO - SPI3_HOST) ---
// ... (pines sin cambios)
#define SD_SPI_SCLK_PIN   GPIO_NUM_47
#define SD_SPI_MOSI_PIN   GPIO_NUM_21
#define SD_SPI_MISO_PIN   GPIO_NUM_2
#define SD_CS_PIN         GPIO_NUM_1

// --- PANTALLA ---
// ... (pines sin cambios)
#define TFT_RST   GPIO_NUM_12
#define TFT_DC    GPIO_NUM_11
#define TFT_CS    GPIO_NUM_10
#define TFT_BL    GPIO_NUM_9

// --- BOTONES ---
// ... (pines sin cambios)
#define BUTTON_ON_OFF_PIN  GPIO_NUM_8
#define BUTTON_LEFT_PIN    GPIO_NUM_7
#define BUTTON_CANCEL_PIN  GPIO_NUM_6
#define BUTTON_OK_PIN      GPIO_NUM_5
#define BUTTON_RIGHT_PIN   GPIO_NUM_4

// --- LEDS ---
#define RGB_LED_PIN      GPIO_NUM_48

// --- AUDIO ---
#define BUZZER_PIN   GPIO_NUM_18

// Pines I2S Unificados para Audio In/Out
#define I2S_BCLK_PIN   GPIO_NUM_16
#define I2S_WS_PIN     GPIO_NUM_17
#define I2S_DOUT_PIN   GPIO_NUM_15 
#define I2S_DIN_PIN    GPIO_NUM_38 

// --- CONFIGURACIÓN DE PANTALLA Y SPI ---
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240
#define LCD_HOST      SPI2_HOST
#define SD_HOST       SPI3_HOST
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define LVGL_TICK_PERIOD_MS 10

// --- [FIX] CONFIGURACIÓN DE AUDIO ---
#define MAX_VOLUME_PERCENTAGE 35 // Límite de volumen (0-100) para proteger el altavoz

#endif // CONFIG_H