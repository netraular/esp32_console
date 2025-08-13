#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <driver/gpio.h>

// --- DISPLAY SPI BUS (SPI2_HOST) ---
#define TFT_BL          GPIO_NUM_4
#define TFT_CS          GPIO_NUM_5
#define TFT_DC          GPIO_NUM_6
#define TFT_RST         GPIO_NUM_7
#define SPI_MOSI_PIN    GPIO_NUM_15
#define SPI_SCLK_PIN    GPIO_NUM_16
#define SPI_MISO_PIN    GPIO_NUM_NC


// --- I2S Pins for Audio Input (INMP441 Microphone) ---
#define I2S_MIC_WS_PIN         GPIO_NUM_17
#define I2S_MIC_BCLK_PIN       GPIO_NUM_18
#define I2S_MIC_DIN_PIN        GPIO_NUM_8


// --- BUTTON PINS ---
#define BUTTON_ON_OFF_PIN  GPIO_NUM_14
#define BUTTON_LEFT_PIN    GPIO_NUM_13
#define BUTTON_CANCEL_PIN  GPIO_NUM_12
#define BUTTON_OK_PIN      GPIO_NUM_11
#define BUTTON_RIGHT_PIN   GPIO_NUM_10


// --- DEDICATED SD CARD SPI BUS (SPI3_HOST) ---
#define SD_CS_PIN         GPIO_NUM_41
#define SD_SPI_MOSI_PIN   GPIO_NUM_40
#define SD_SPI_SCLK_PIN   GPIO_NUM_39
#define SD_SPI_MISO_PIN   GPIO_NUM_38



// --- I2S Pins for Audio Output (MAX98357A Speaker) ---
#define I2S_SPEAKER_WS_PIN     GPIO_NUM_1 //LRC
#define I2S_SPEAKER_BCLK_PIN   GPIO_NUM_2
#define I2S_SPEAKER_DOUT_PIN   GPIO_NUM_42


// --- RGB LED PINS ---
#define RGB_LED_PIN      GPIO_NUM_48


//Libres: 9,3,21,47 (1 para batería, otro para volumen)
//No usar: 19,20 (usb) 0 (rtc), 35,36,37 (psram) 45,46
#endif // BOARD_CONFIG_H