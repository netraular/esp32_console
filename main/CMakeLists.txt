# CMakeLists.txt para el componente principal 'main'

idf_component_register(
    # --- 1. Archivos fuente a compilar ---
    SRCS
        "main.cpp"
        # "controllers/sd_card_manager/sd_card_manager.cpp"  <-- ELIMINA ESTA LÍNEA
        "controllers/screen_manager/screen_manager.cpp"      # Asumo que aún tienes este

    # --- 2. Directorios donde buscar archivos de cabecera (.h) ---
    INCLUDE_DIRS
        "." 
        "../managed_components/lvgl__lvgl/src/libs/lodepng"

    # --- 3. Dependencias de otros componentes ---
    # ¡ESTAS DEPENDENCIAS AÚN SON NECESARIAS!
    PRIV_REQUIRES
        heap
        log
        freertos
        lvgl
        esp_lcd
        driver
        sdmmc
        fatfs
)