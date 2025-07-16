#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configura el botón ON/OFF como fuente de despertar y entra en modo Light Sleep.
 * 
 * Esta función realiza los siguientes pasos:
 * 1. Configura el pin del botón ON/OFF para que despierte el ESP32 en un nivel bajo.
 * 2. Llama a `esp_light_sleep_start()` para poner el dispositivo en modo de bajo consumo.
 * 3. Al despertar, registra la causa y deshabilita la configuración de despertar GPIO.
 * 4. Incluye un pequeño retardo para "debounce" el botón de despertar y evitar eventos
 *    no deseados en el button_manager al reanudar.
 */
void power_manager_enter_light_sleep(void);


#ifdef __cplusplus
}
#endif

#endif // POWER_MANAGER_H