/**
 * @file main.cpp
 * @brief Aplicación consolidada que gestiona la pantalla, LVGL y la tarjeta SD.
 */

// --- HEADERS NECESARIOS ---
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
#include <cassert>
#include <cstring>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h> 

// Headers para el controlador de pantalla
#include "config.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"

// extern "C" es necesario para incluir headers de C en un archivo C++
extern "C" {
#include "lvgl.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
}

// --- SECCIÓN DE GESTIÓN DE TARJETA SD (integrada en main.cpp) ---

static const char* TAG_SD = "SD_CARD"; // Tag para los logs de la SD
static bool s_bus_initialized = false;
static bool s_is_mounted = false;
static sdmmc_card_t* s_card;
static const char* MOUNT_POINT = "/sdcard";

/** @brief Inicializa el bus SPI dedicado para la tarjeta SD. */
static bool sd_init(void) {
    if (s_bus_initialized) {
        ESP_LOGW(TAG_SD, "SPI bus already initialized.");
        return true;
    }
    ESP_LOGI(TAG_SD, "Initializing SPI bus for SD card (SPI3_HOST)");
    
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = SD_SPI_MOSI_PIN;
    bus_cfg.miso_io_num = SD_SPI_MISO_PIN;
    bus_cfg.sclk_io_num = SD_SPI_SCLK_PIN;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4096;

    esp_err_t ret = spi_bus_initialize(SD_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_SD, "Failed to initialize SPI bus. Error: %s", esp_err_to_name(ret));
        return false;
    }
    s_bus_initialized = true;
    return true;
}

/** @brief Monta el sistema de archivos FAT de la tarjeta SD. */
static bool sd_mount(void) {
    if (s_is_mounted) {
        return true;
    }
    if (!s_bus_initialized) {
        ESP_LOGE(TAG_SD, "SPI bus has not been initialized. Call sd_init() first.");
        return false;
    }
    ESP_LOGI(TAG_SD, "Attempting to mount SD card...");
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_HOST;
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = SD_HOST;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 10;
    mount_config.allocation_unit_size = 16 * 1024;

    esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG_SD, "Failed to mount filesystem. It may be missing a FAT format.");
        } else {
            ESP_LOGE(TAG_SD, "Failed to initialize card (%s).", esp_err_to_name(ret));
        }
        s_is_mounted = false;
        return false;
    }
    ESP_LOGI(TAG_SD, "SD Card mounted successfully at %s", MOUNT_POINT);
    sdmmc_card_print_info(stdout, s_card);
    s_is_mounted = true;
    return true;
}

/** @brief Lee el contenido completo de un archivo. El llamador debe liberar el buffer. */
static bool sd_read_file(const char* path, char** buffer, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG_SD, "Failed to open file for reading: %s. Error: %s", path, strerror(errno));
        return false;
    }

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    *buffer = (char*)malloc(*size + 1);
    if (!*buffer) {
        ESP_LOGE(TAG_SD, "Failed to allocate memory for file content");
        fclose(f);
        return false;
    }

    if (fread(*buffer, 1, *size, f) != *size) {
        ESP_LOGE(TAG_SD, "Failed to read full file content");
        free(*buffer);
        *buffer = NULL;
        fclose(f);
        return false;
    }

    (*buffer)[*size] = '\0';
    fclose(f);
    ESP_LOGI(TAG_SD, "Read %zu bytes from %s", *size, path);
    return true;
}

// --- FIN SECCIÓN DE GESTIÓN DE TARJETA SD ---


// --- SECCIÓN PANTALLA Y LVGL ---

static const char *TAG_MAIN = "MAIN_APP"; // Tag para los logs principales

typedef struct {
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_handle_t panel_handle;
    lv_display_t *lvgl_disp;
    lv_color_t *lvgl_buf1;
    lv_color_t *lvgl_buf2;
} screen_t;

static esp_timer_handle_t lv_tick_timer = nullptr;

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    screen_t* s = (screen_t*)lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(s->panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

static void screen_init_lvgl_tick() {
    esp_timer_create_args_t lv_tick_timer_args = {};
    lv_tick_timer_args.callback = [](void* arg) { lv_tick_inc(LVGL_TICK_PERIOD_MS); };
    lv_tick_timer_args.name = "lv_tick";
    ESP_ERROR_CHECK(esp_timer_create(&lv_tick_timer_args, &lv_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lv_tick_timer, LVGL_TICK_PERIOD_MS * 1000));
}

static screen_t* screen_init() {
    ESP_LOGI(TAG_MAIN, "Inicializando hardware de la pantalla");
    screen_t* screen = new screen_t;
    if (!screen) {
        ESP_LOGE(TAG_MAIN, "Fallo en la asignación de memoria para la pantalla");
        return nullptr;
    }

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = SPI_MOSI_PIN;
    buscfg.miso_io_num = SPI_MISO_PIN;
    buscfg.sclk_io_num = SPI_SCLK_PIN;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = SCREEN_WIDTH * 40 * sizeof(lv_color_t);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = TFT_CS;
    io_config.dc_gpio_num = TFT_DC;
    io_config.spi_mode = 0;
    io_config.pclk_hz = LCD_PIXEL_CLOCK_HZ;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &screen->io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
        .bits_per_pixel = 16,
        .flags = { .reset_active_high = 0, },
        .vendor_config = nullptr,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(screen->io_handle, &panel_config, &screen->panel_handle));

    esp_lcd_panel_reset(screen->panel_handle);
    esp_lcd_panel_init(screen->panel_handle);
    esp_lcd_panel_invert_color(screen->panel_handle, true);
    esp_lcd_panel_disp_on_off(screen->panel_handle, true);

    gpio_config_t bk_gpio_config = {};
    bk_gpio_config.pin_bit_mask = 1ULL << TFT_BL;
    bk_gpio_config.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(TFT_BL, 1);

    lv_init();
    screen_init_lvgl_tick();

    screen->lvgl_buf1 = (lv_color_t*)heap_caps_malloc(SCREEN_WIDTH * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    screen->lvgl_buf2 = (lv_color_t*)heap_caps_malloc(SCREEN_WIDTH * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(screen->lvgl_buf1 && screen->lvgl_buf2);

    screen->lvgl_disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_buffers(screen->lvgl_disp, screen->lvgl_buf1, screen->lvgl_buf2,
                          SCREEN_WIDTH * 40 * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(screen->lvgl_disp, lvgl_flush_cb);
    lv_display_set_user_data(screen->lvgl_disp, screen);
    ESP_LOGI(TAG_MAIN, "LVGL inicializado correctamente");

    return screen;
}

static lv_obj_t* counter_label;

static void create_counter_ui(lv_obj_t* parent) {
    counter_label = lv_label_create(parent);
    lv_label_set_text(counter_label, "0");
    lv_obj_set_style_text_font(counter_label, &lv_font_montserrat_48, 0);
    lv_obj_center(counter_label);
    ESP_LOGI(TAG_MAIN, "UI del contador creada.");
}

// --- FIN SECCIÓN PANTALLA Y LVGL ---


// --- PUNTO DE ENTRADA DE LA APLICACIÓN ---
extern "C" void app_main(void) {
    ESP_LOGI(TAG_MAIN, "--- Iniciando aplicación ---");

    // --- 1. Inicializar la tarjeta SD y leer el archivo text.txt ---
    if (sd_init() && sd_mount()) {
        char file_path[256];
        snprintf(file_path, sizeof(file_path), "%s/text.txt", MOUNT_POINT);
        
        ESP_LOGI(TAG_MAIN, "Intentando leer el archivo: %s", file_path);

        char* file_content = NULL;
        size_t file_size = 0;

        if (sd_read_file(file_path, &file_content, &file_size)) {
            ESP_LOGI(TAG_MAIN, "Lectura de archivo exitosa (%zu bytes).", file_size);
            ESP_LOGI(TAG_MAIN, "================== CONTENIDO DE text.txt ==================");
            printf("%s\n", file_content);
            ESP_LOGI(TAG_MAIN, "==========================================================");
            free(file_content);
        } else {
            ESP_LOGE(TAG_MAIN, "Fallo al leer el archivo '%s'.", file_path);
        }
    } else {
        ESP_LOGE(TAG_MAIN, "Fallo al inicializar o montar la tarjeta SD.");
    }

    // --- 2. Inicializar la pantalla y LVGL ---
    screen_t* screen = screen_init();
    if (!screen) {
        ESP_LOGE(TAG_MAIN, "Fallo al inicializar la pantalla. Deteniendo.");
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    // --- 3. Crear la interfaz de usuario ---
    create_counter_ui(lv_screen_active());

    // --- 4. Bucle principal ---
    ESP_LOGI(TAG_MAIN, "Entrando en el bucle principal.");
    static int counter = 0;
    char buf[16];

    while (true) {
        snprintf(buf, sizeof(buf), "%d", counter++);
        lv_label_set_text(counter_label, buf);
        lv_timer_handler(); 
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}