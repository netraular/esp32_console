# Gestor de Pantalla (Screen Manager)

## Descripción
Este componente se encarga de toda la inicialización de bajo nivel para la pantalla del dispositivo. Configura el bus SPI, el controlador de la pantalla (ST7789) y la librería gráfica LVGL, actuando como el puente fundamental entre el software de la interfaz de usuario y el hardware físico.

## Características
- **Inicialización del Bus SPI:** Configura y arranca el bus `SPI2_HOST` dedicado para la comunicación con la pantalla.
- **Controlador de Pantalla:** Inicializa el driver del panel LCD ST7789, incluyendo la configuración de pines (CS, DC, RST) y los parámetros de comunicación.
- **Control de Backlight:** Gestiona el pin de la luz de fondo (`TFT_BL`) para encenderla durante la inicialización.
- **Integración con LVGL:**
    - Inicializa la librería LVGL.
    - Crea y configura un `esp_timer` periódico para proveer el "tick" necesario para el funcionamiento de LVGL (`lv_tick_inc`).
    - Asigna dos búferes de dibujado en memoria RAM apta para DMA, permitiendo un renderizado eficiente en modo parcial.
    - Proporciona la función de callback `lvgl_flush_cb` que transfiere los datos renderizados por LVGL a la pantalla física.

## Modo de Uso

1.  **Inicializar el Gestor:**
    Llama a esta función una sola vez al inicio de la aplicación. Devuelve un puntero a una estructura `screen_t` que contiene los handles necesarios, o `nullptr` si falla.
    ```cpp
    #include "controllers/screen_manager/screen_manager.h"

    // En tu función app_main
    screen_t* screen = screen_init();
    if (!screen) {
        ESP_LOGE(TAG, "Fallo al inicializar la pantalla, deteniendo.");
        return;
    }
    ```

2.  **Manejar el Bucle de LVGL:**
    Después de la inicialización, el bucle principal de la aplicación debe llamar a `lv_timer_handler()` periódicamente para que LVGL procese sus tareas (como animaciones y eventos).
    ```cpp
    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ```

## Dependencias
-   **ESP-IDF:** Componentes `esp_lcd` y `driver/spi_master`.
-   **Librerías de Terceros:** `lvgl/lvgl`.
-   **Configuración Local:** Requiere que los pines de la pantalla (`SPI_SCLK_PIN`, `SPI_MOSI_PIN`, `TFT_CS`, `TFT_DC`, `TFT_RST`, `TFT_BL`) estén correctamente definidos en `config.h`.