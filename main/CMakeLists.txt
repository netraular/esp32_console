# --- CMakeLists.txt para el componente 'main' (versión Sandbox Limpia y Corregida) ---

idf_component_register(
    # 1. Archivos fuente (.cpp) requeridos para este sandbox
    SRCS
        "main.cpp"
        # Controladores esenciales para la pantalla y los botones
        "./controllers/screen_manager/screen_manager.cpp"
        "./controllers/button_manager/button_manager.cpp"

    # 2. Directorios para encontrar archivos .h
    INCLUDE_DIRS
        "."  # Permite #include "controllers/..."
        # Ruta al header de lodepng dentro del componente gestionado por ESP-IDF
        "../managed_components/lvgl__lvgl/src/libs/lodepng"

    # 3. Archivos binarios para incrustar en el firmware
    EMBED_FILES
        "./assets/my_image_240.png"

    # 4. Dependencias de otros componentes (públicos y de ESP-IDF)
    PRIV_REQUIRES
        # Dependencias de ESP-IDF que nuestro main.cpp utiliza directamente
        heap          # Para heap_caps.h y gestión de memoria (PSRAM)
        log           # Para ESP_LOGI, ESP_LOGE, etc.
        nvs_flash     # Para nvs_flash_init()
        esp_event     # Para el bucle de eventos por defecto
        esp_netif     # Para esp_netif_init()
        freertos      # Para FreeRTOS (vTaskDelay, etc.)

        # Dependencias para los controladores de hardware específicos
        esp_lcd       # Para los headers esp_lcd_panel_io.h, etc., usados por screen_manager

        # Dependencias de componentes de terceros o del proyecto
        lvgl          # Dependencia explícita de LVGL
)

# 5. Especificación de la tabla de particiones
idf_build_set_property(PARTITION_TABLE_FILENAME partitions.csv)