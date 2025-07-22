/**
 * @file main.cpp
 * @brief Aplicación que muestra un contador en la pantalla.
 *        Toda la inicialización de la pantalla y LVGL está contenida en este archivo.
 */

// --- HEADERS NECESARIOS ---
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>    // Para snprintf
#include <cassert>    // Para assert

// Headers que antes estaban en screen_manager
#include "config.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"

// extern "C" es necesario para incluir headers de C (como LVGL) en un archivo C++
extern "C" {
#include "lvgl.h"
}

// Tag para los logs
static const char *TAG = "CONTADOR_MAIN";

/**
 * @brief Contiene todos los handles necesarios para gestionar la pantalla y LVGL.
 */
typedef struct {
    esp_lcd_panel_io_handle_t io_handle;   //!< Handle para la comunicación IO del panel (SPI).
    esp_lcd_panel_handle_t panel_handle;   //!< Handle para el driver del panel LCD (ST7789).
    lv_display_t *lvgl_disp;               //!< Puntero al objeto de display de LVGL.
    lv_color_t *lvgl_buf1;                 //!< Puntero al primer buffer de dibujo de LVGL.
    lv_color_t *lvgl_buf2;                 //!< Puntero al segundo buffer de dibujo de LVGL.
} screen_t;

// Timer para el 'tick' de LVGL
static esp_timer_handle_t lv_tick_timer = nullptr;

// Callback para el refresco de pantalla de LVGL
static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    screen_t* s = (screen_t*)lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(s->panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

// Inicializa el timer periódico para LVGL
static void screen_init_lvgl_tick() {
    esp_timer_create_args_t lv_tick_timer_args = {};
    lv_tick_timer_args.callback = [](void* arg) { lv_tick_inc(LVGL_TICK_PERIOD_MS); };
    lv_tick_timer_args.name = "lv_tick";
    ESP_ERROR_CHECK(esp_timer_create(&lv_tick_timer_args, &lv_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lv_tick_timer, LVGL_TICK_PERIOD_MS * 1000));
    ESP_LOGI(TAG, "LVGL tick timer inicializado con %dms de periodo", LVGL_TICK_PERIOD_MS);
}

/**
 * @brief Inicializa el hardware de la pantalla (SPI, LCD) y la librería LVGL.
 *
 * Configura el timer de tick de LVGL, los buffers de dibujo y el callback de refresco.
 * Después de llamar a esta función, el bucle principal debe llamar periódicamente a `lv_timer_handler()`.
 *
 * @return Puntero a una struct screen_t con los handles necesarios, o NULL si falla.
 */
screen_t* screen_init() {
    ESP_LOGI(TAG, "Inicializando hardware de la pantalla");
    screen_t* screen = new screen_t;
    if (!screen) {
        ESP_LOGE(TAG, "Fallo en la asignación de memoria para la pantalla");
        return nullptr;
    }

    // 1. Configuración del bus SPI
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = SPI_MOSI_PIN;
    buscfg.miso_io_num = SPI_MISO_PIN;
    buscfg.sclk_io_num = SPI_SCLK_PIN;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = SCREEN_WIDTH * 40 * sizeof(lv_color_t);

    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "Bus SPI para LCD inicializado");

    // 2. Configuración del panel IO del LCD (SPI)
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = TFT_CS;
    io_config.dc_gpio_num = TFT_DC;
    io_config.spi_mode = 0;
    io_config.pclk_hz = LCD_PIXEL_CLOCK_HZ;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &screen->io_handle));
    ESP_LOGI(TAG, "Panel IO del LCD inicializado");

    // 3. Configuración del driver del panel (ST7789)
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
        .bits_per_pixel = 16,
        .flags = {
            .reset_active_high = 0,
        },
        .vendor_config = nullptr,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(screen->io_handle, &panel_config, &screen->panel_handle));
    ESP_LOGI(TAG, "Driver del panel ST7789 inicializado");

    // 4. Inicialización del panel
    esp_lcd_panel_reset(screen->panel_handle);
    esp_lcd_panel_init(screen->panel_handle);
    esp_lcd_panel_invert_color(screen->panel_handle, true);
    esp_lcd_panel_disp_on_off(screen->panel_handle, true);

    // 5. Configuración del backlight
    gpio_config_t bk_gpio_config = {};
    bk_gpio_config.pin_bit_mask = 1ULL << TFT_BL;
    bk_gpio_config.mode = GPIO_MODE_OUTPUT;
    
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(TFT_BL, 1); // Encender backlight
    ESP_LOGI(TAG, "Backlight activado");

    // 6. Inicialización de LVGL
    ESP_LOGI(TAG, "Inicializando LVGL");
    lv_init();
    screen_init_lvgl_tick();

    // Asignar buffers de dibujo para LVGL en memoria capaz para DMA
    screen->lvgl_buf1 = (lv_color_t*)heap_caps_malloc(SCREEN_WIDTH * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    screen->lvgl_buf2 = (lv_color_t*)heap_caps_malloc(SCREEN_WIDTH * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(screen->lvgl_buf1 && screen->lvgl_buf2);

    // Crear y configurar el display de LVGL
    screen->lvgl_disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_buffers(screen->lvgl_disp, screen->lvgl_buf1, screen->lvgl_buf2,
                          SCREEN_WIDTH * 40 * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(screen->lvgl_disp, lvgl_flush_cb);
    lv_display_set_user_data(screen->lvgl_disp, screen);
    ESP_LOGI(TAG, "LVGL inicializado correctamente");

    return screen;
}

/**
 * @brief Desinicializa la pantalla, LVGL, y libera todos los recursos asociados.
 *
 * @param screen Puntero a la struct screen_t devuelta por screen_init().
 */
void screen_deinit(screen_t* screen) {
    if (lv_tick_timer) {
        esp_timer_stop(lv_tick_timer);
        esp_timer_delete(lv_tick_timer);
        lv_tick_timer = nullptr;
    }
    if (screen) {
        if (screen->panel_handle) esp_lcd_panel_del(screen->panel_handle);
        if (screen->io_handle) esp_lcd_panel_io_del(screen->io_handle);
        if (screen->lvgl_buf1) heap_caps_free(screen->lvgl_buf1);
        if (screen->lvgl_buf2) heap_caps_free(screen->lvgl_buf2);
        delete screen;
        ESP_LOGI(TAG, "Pantalla desinicializada");
    }
}

// --- FIN DEL CÓDIGO INTEGRADO ---


// Puntero global a la etiqueta del contador para poder actualizarla
static lv_obj_t* counter_label;

/**
 * @brief Crea la etiqueta del contador en la pantalla.
 */
void create_counter_ui(lv_obj_t* parent) {
    counter_label = lv_label_create(parent);
    lv_label_set_text(counter_label, "0");
    lv_obj_set_style_text_font(counter_label, &lv_font_montserrat_48, 0);
    lv_obj_center(counter_label);
    ESP_LOGI(TAG, "UI del contador creada.");
}


// --- PUNTO DE ENTRADA DE LA APLICACIÓN ---
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "--- Iniciando aplicación 'Contador' ---");

    // --- 1. Inicializar la pantalla y LVGL ---
    screen_t* screen = screen_init();
    if (!screen) {
        ESP_LOGE(TAG, "Fallo al inicializar la pantalla. Deteniendo.");
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    ESP_LOGI(TAG, "Controlador de pantalla inicializado.");

    // --- 2. Crear la interfaz de usuario ---
    create_counter_ui(lv_screen_active());

    // --- 3. Bucle principal con la lógica del contador ---
    ESP_LOGI(TAG, "Entrando en el bucle principal.");
    
    static int counter = 0;
    char buf[16];

    while (true) {
        // Actualizar el texto del contador
        snprintf(buf, sizeof(buf), "%d", counter);
        lv_label_set_text(counter_label, buf);
        
        // Incrementar el contador
        counter++;

        // Procesar tareas de LVGL (muy importante para redibujar)
        // Usamos un delay más corto aquí para que la UI sea más fluida
        // si se añaden más elementos en el futuro.
        lv_timer_handler(); 
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Actualizamos el contador solo una vez por segundo
        static uint32_t last_update = 0;
        if (esp_log_timestamp() - last_update > 1000) {
            last_update = esp_log_timestamp();
            // La lógica del contador ya no necesita estar aquí,
            // podemos simplificar el bucle.
        }
    }
}