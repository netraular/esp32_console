# --- Detección Automática de Ficheros Fuente ---
# Busca recursivamente todos los ficheros .c y .cpp dentro del directorio 'main'
# y sus subdirectorios. Esto elimina la necesidad de añadir manualmente cada
# nuevo fichero a la lista de SRCS.
file(GLOB_RECURSE MAIN_SRCS 
    "*.cpp"
    "*.c"
)

# --- Explicitly Add New Files ---
# This ensures that new source files are included in the build even if the GLOB
# command hasn't been re-run, which resolves potential linker errors.
list(APPEND MAIN_SRCS
    "views/core/settings_view/settings_view.cpp"
    "controllers/pet_manager/pet_manager.cpp"
    "views/game/pet_view/pet_view.cpp"
    "views/game/pet_collection_view/pet_collection_view.cpp"
    "views/game/pet_hub_view/pet_hub_view.cpp"
    "components/memory_monitor_component/memory_monitor_component.cpp"
)
list(REMOVE_DUPLICATES MAIN_SRCS)


# --- Registro del Componente 'main' ---
idf_component_register(
    # Variable que ahora contiene todos nuestros ficheros fuente
    SRCS ${MAIN_SRCS}

    INCLUDE_DIRS "." "models"
    
    # Dependencias para el componente 'main'
    REQUIRES 
        # --- Componentes del Registro IDF (de idf_component.yml) ---
        button              # Para iot_button en button_manager
        littlefs            # Para esp_littlefs en littlefs_manager

        # --- Componentes del Sistema ESP-IDF (usados por los controladores) ---
        driver              # Para GPIO, SPI, I2S
        esp_wifi            # Para wifi_manager
        esp_netif           # Para wifi_manager
        esp_event           # Para wifi_manager
        esp_lcd             # Para screen_manager
        nvs_flash           # Para data_manager y wifi_manager
        fatfs               # PARA sd_card_manager (esp_vfs_fat.h)
        sdmmc               # PARA sd_card_manager
        esp_http_client     # PARA stt_manager y weather_manager
        esp-tls             # PARA stt_manager (HTTPS) y weather_manager
        esp_system          # PARA power_manager (esp_sleep.h)
        esp_rom             # PARA funciones ROM (ej. en audio_manager)
        json                # PARA stt_manager (cJSON.h) Y notification_manager

    EMBED_FILES 
        "./server_certs/groq_api_ca.pem"
        "./server_certs/open_meteo_ca.pem"
        "./littlefs/welcome.txt"
)

idf_build_set_property(PARTITION_TABLE_FILENAME partitions.csv)