#ifndef CONFIG_H
#define CONFIG_H

#include <driver/gpio.h>

// --- BUS SPI PANTALLA (SPI2_HOST) ---
// Estos pines son solo para la pantalla.
#define SPI_SCLK_PIN    GPIO_NUM_14
#define SPI_MOSI_PIN    GPIO_NUM_13
#define SPI_MISO_PIN    GPIO_NUM_NC // La pantalla no usa MISO, así que lo ponemos como No Conectado

// --- BUS SPI TARJETA SD (NUEVO, DEDICADO - SPI3_HOST) ---
#define SD_SPI_SCLK_PIN   GPIO_NUM_47 // Anteriormente RED_LED_PIN
#define SD_SPI_MOSI_PIN   GPIO_NUM_21 // Anteriormente GREEN_LED_PIN
#define SD_SPI_MISO_PIN   GPIO_NUM_2  // Este pin ya estaba dedicado a MISO
#define SD_CS_PIN         GPIO_NUM_1  // Chip Select específico para la tarjeta SD (sin cambios)

// --- PANTALLA ---
#define TFT_RST   GPIO_NUM_12
#define TFT_DC    GPIO_NUM_11
#define TFT_CS    GPIO_NUM_10 // Chip Select específico para la pantalla
#define TFT_BL    GPIO_NUM_9

// --- BOTONES ---
#define BUTTON_ON_OFF_PIN  GPIO_NUM_8
#define BUTTON_LEFT_PIN    GPIO_NUM_7
#define BUTTON_CANCEL_PIN  GPIO_NUM_6
#define BUTTON_OK_PIN      GPIO_NUM_5
#define BUTTON_RIGHT_PIN   GPIO_NUM_4

// --- LEDS ---
// El pin del RGB LED (GPIO47) es el NeoPixel integrado en la placa
#define RGB_LED_PIN      GPIO_NUM_48

// --- AUDIO ---
#define BUZZER_PIN   GPIO_NUM_18

// Pines I2S Unificados para Audio In/Out
#define I2S_BCLK_PIN   GPIO_NUM_16
#define I2S_WS_PIN     GPIO_NUM_17
#define I2S_DOUT_PIN   GPIO_NUM_15 // -> MAX98357A (altavoz)
#define I2S_DIN_PIN    GPIO_NUM_38 // <- INMP441 (micrófono)

// --- CONFIGURACIÓN DE PANTALLA Y SPI ---
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240
#define LCD_HOST      SPI2_HOST // Bus para la pantalla
#define SD_HOST       SPI3_HOST // Bus dedicado para la tarjeta SD
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000) // 40 MHz
#define LVGL_TICK_PERIOD_MS 10

#endif // CONFIG_H