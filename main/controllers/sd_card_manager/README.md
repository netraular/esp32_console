# SD Card Manager

## Descripción
Este componente gestiona el acceso a una tarjeta Micro SD a través del bus SPI. Está diseñado para funcionar en un bus SPI compartido con otros periféricos (como una pantalla).

## Características
- Inicializa y monta la tarjeta SD en un bus SPI ya existente.
- Proporciona funciones para comprobar el estado de montaje, listar archivos y desmontar la tarjeta.
- Utiliza el sistema de archivos FAT a través de VFS de ESP-IDF.

## Uso

1.  **Inicializar el bus SPI:**
    Asegúrate de que el bus SPI (definido por `LCD_HOST`) se inicializa *antes* de llamar al gestor de la SD. Normalmente, el `screen_manager` hace esto.

2.  **Inicializar el gestor de la SD:**
    Llama a esta función después de inicializar el bus SPI.
    ```cpp
    #include "controllers/sd_card_manager/sd_card_manager.h"
    
    // ...
    // screen_init(); // Esto inicializa el bus SPI
    if (sd_manager_init()) {
        // La tarjeta está lista
    } else {
        // Falló el montaje
    }
    ```

3.  **Listar archivos:**
    ```cpp
    if (sd_manager_is_mounted()) {
        sd_manager_list_files(sd_manager_get_mount_point());
    }
    ```