# --- Detección Automática de Ficheros Fuente ---
# Busca recursivamente todos los ficheros .c y .cpp dentro del directorio 'main'
# y sus subdirectorios. Esto elimina la necesidad de añadir manualmente cada
# nuevo fichero a la lista de SRCS.
file(GLOB_RECURSE MAIN_SRCS 
    "*.cpp"
    "*.c"
)

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
        json_parser         # <<< PARA notification_manager
        json_generator      # <<< PARA notification_manager

        # --- Componentes del Sistema ESP-IDF (usados por los controladores) ---
        driver              # Para GPIO, SPI, I2S
        esp_wifi            # Para wifi_manager
        esp_netif           # Para wifi_manager
        esp_event           # Para wifi_manager
        esp_lcd             # Para screen_manager
        nvs_flash           # Para data_manager y wifi_manager
        fatfs               # PARA sd_card_manager (esp_vfs_fat.h)
        sdmmc               # PARA sd_card_manager
        esp_http_client     # PARA stt_manager
        esp-tls             # PARA stt_manager (HTTPS)
        esp_system          # PARA power_manager (esp_sleep.h)
        esp_rom             # PARA funciones ROM (ej. en audio_manager)
        json                # PARA stt_manager (cJSON.h)

    EMBED_FILES 
        "./server_certs/groq_api_ca.pem"
        "./littlefs/welcome.txt"
)

idf_build_set_property(PARTITION_TABLE_FILENAME partitions.csv)