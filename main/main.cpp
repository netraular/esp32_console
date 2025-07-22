/**
 * @file main.cpp
 * @brief Aplicación consolidada que gestiona la pantalla, LVGL, la tarjeta SD y muestra una imagen.
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


// --- SECCIÓN DRIVER VFS DE LVGL ---
// Este driver actúa como un puente entre LVGL y el sistema de archivos VFS de ESP-IDF (FAT).
// LVGL usa una letra de unidad (ej. 'S') para acceder a los archivos.
static const char *TAG_LVGL_VFS = "LVGL_VFS";

static void * fs_open_cb(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    const char * mode_str;
    if (mode == LV_FS_MODE_WR) mode_str = "wb";
    else if (mode == LV_FS_MODE_RD) mode_str = "rb";
    else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) mode_str = "rb+";
    else {
        ESP_LOGE(TAG_LVGL_VFS, "Unknown file open mode: %d", mode);
        return NULL;
    }
    // LVGL elimina la letra de la unidad, por lo que 'path' es la ruta completa (ej. "/sdcard/image.png")
    FILE * f = fopen(path, mode_str);
    if (f == NULL) {
        ESP_LOGE(TAG_LVGL_VFS, "Failed to open file: %s (mode: %s)", path, mode_str);
        return NULL;
    }
    return f;
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t * drv, void * file_p) {
    FILE * f = (FILE *)file_p;
    if (f) fclose(f);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read_cb(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
    FILE * f = (FILE *)file_p;
    *br = fread(buf, 1, btr, f);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_write_cb(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw) {
    FILE * f = (FILE *)file_p;
    *bw = fwrite(buf, 1, btw, f);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek_cb(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
    FILE * f = (FILE *)file_p;
    int seek_mode;
    switch (whence) {
        case LV_FS_SEEK_SET: seek_mode = SEEK_SET; break;
        case LV_FS_SEEK_CUR: seek_mode = SEEK_CUR; break;
        case LV_FS_SEEK_END: seek_mode = SEEK_END; break;
        default: return LV_FS_RES_INV_PARAM;
    }
    return (fseek(f, pos, seek_mode) == 0) ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t fs_tell_cb(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
    FILE * f = (FILE *)file_p;
    long pos = ftell(f);
    if (pos == -1) return LV_FS_RES_FS_ERR;
    *pos_p = (uint32_t)pos;
    return LV_FS_RES_OK;
}

/** @brief Inicializa y registra el driver del sistema de archivos para LVGL. */
static void lvgl_fs_driver_init(char drive_letter) {
    static lv_fs_drv_t fs_drv; // Debe ser estática o global
    lv_fs_drv_init(&fs_drv);

    fs_drv.letter = drive_letter;
    fs_drv.open_cb = fs_open_cb;
    fs_drv.close_cb = fs_close_cb;
    fs_drv.read_cb = fs_read_cb;
    fs_drv.write_cb = fs_write_cb;
    fs_drv.seek_cb = fs_seek_cb;
    fs_drv.tell_cb = fs_tell_cb;
    
    // Estos callbacks son opcionales y solo necesarios si usas las funciones de directorio de LVGL
    fs_drv.dir_open_cb = NULL;
    fs_drv.dir_read_cb = NULL;
    fs_drv.dir_close_cb = NULL;

    lv_fs_drv_register(&fs_drv);
    ESP_LOGI(TAG_LVGL_VFS, "Driver de sistema de archivos LVGL registrado para la unidad '%c'", drive_letter);
}

// --- FIN SECCIÓN DRIVER VFS DE LVGL ---


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
    
    // >>> NUEVO: Inicializar el driver VFS para que LVGL pueda leer desde la SD
    // Usamos 'S' como la letra de unidad para la tarjeta SD.
    lvgl_fs_driver_init('S');

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

// >>> MODIFICADO: Ahora crea el contador y la imagen
static void create_main_ui(lv_obj_t* parent) {
    // --- 1. Crear y posicionar la etiqueta del contador ---
    counter_label = lv_label_create(parent);
    lv_label_set_text(counter_label, "0");
    lv_obj_set_style_text_font(counter_label, &lv_font_montserrat_48, 0);
    // Alinear en la parte superior central con un margen de 20px
    lv_obj_align(counter_label, LV_ALIGN_TOP_MID, 0, 20);
    ESP_LOGI(TAG_MAIN, "UI del contador creada.");

    // --- 2. Crear y posicionar el widget de la imagen ---
    // LVGL necesita una ruta con una letra de unidad. La hemos registrado como 'S'.
    // El formato es "S:<punto_de_montaje>/<nombre_archivo>"
    // NOTA: Asegúrate de que tu componente LVGL esté compilado con soporte para PNG.
    // Esto se suele activar en menuconfig -> Component config -> LVGL -> LVGL libraries -> Image decoder -> Enable PNG decoder
    char img_path[256];
    snprintf(img_path, sizeof(img_path), "S:%s/image.png", MOUNT_POINT);

    ESP_LOGI(TAG_MAIN, "Intentando cargar la imagen desde: %s", img_path);

    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, img_path);
    // Alinear la imagen debajo de la etiqueta del contador, con un margen de 10px
    lv_obj_align_to(img, counter_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    // Si la imagen no se puede cargar (no existe, formato incorrecto, etc.),
    // LVGL mostrará un símbolo de "archivo roto".
    // Puedes comprobar si el archivo existe antes si quieres un manejo de errores más robusto.
}

// --- FIN SECCIÓN PANTALLA Y LVGL ---


// --- PUNTO DE ENTRADA DE LA APLICACIÓN ---
extern "C" void app_main(void) {
    ESP_LOGI(TAG_MAIN, "--- Iniciando aplicación ---");

    // --- 1. Inicializar la tarjeta SD ---
    // Esto es crucial y debe hacerse ANTES de crear la UI que depende de la SD.
    if (sd_init() && sd_mount()) {
        ESP_LOGI(TAG_MAIN, "Tarjeta SD inicializada y montada correctamente.");
        // Opcional: podemos seguir leyendo el text.txt como prueba
        char file_path[256];
        snprintf(file_path, sizeof(file_path), "%s/text.txt", MOUNT_POINT);
        char* file_content = NULL;
        size_t file_size = 0;
        if (sd_read_file(file_path, &file_content, &file_size)) {
            ESP_LOGI(TAG_MAIN, "Prueba de lectura de text.txt exitosa.");
            free(file_content);
        }
    } else {
        ESP_LOGE(TAG_MAIN, "Fallo al inicializar o montar la tarjeta SD. La imagen no se cargará.");
        // La aplicación puede continuar, pero la imagen no aparecerá.
    }

    // --- 2. Inicializar la pantalla y LVGL ---
    screen_t* screen = screen_init();
    if (!screen) {
        ESP_LOGE(TAG_MAIN, "Fallo al inicializar la pantalla. Deteniendo.");
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    // --- 3. Crear la interfaz de usuario (contador e imagen) ---
    create_main_ui(lv_screen_active());

    // --- 4. Bucle principal ---
    ESP_LOGI(TAG_MAIN, "Entrando en el bucle principal.");
    static int counter = 0;
    char buf[16];

    while (true) {
        snprintf(buf, sizeof(buf), "%d", counter++);
        // Actualizar el texto de la etiqueta en el bucle
        if(counter_label) {
             lv_label_set_text(counter_label, buf);
        }
       
        // Dejar que LVGL gestione sus tareas (renderizado, eventos, etc.)
        lv_timer_handler(); 
        
        // Un pequeño retardo para ceder el control a otras tareas
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}