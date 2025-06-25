#include "wifi_manager.h"
#include "secret.h" // <-- para credenciales
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "WIFI_MGR";

// Event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

// Store the IP address
static esp_ip4_addr_t s_ip_address;
static bool s_is_connected = false;
static bool s_is_initialized = false; // <-- para controlar el estado

// Handles para poder anular el registro de eventos
static esp_event_handler_instance_t s_instance_any_id;
static esp_event_handler_instance_t s_instance_got_ip;
static esp_netif_t *s_sta_netif = NULL;


static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_is_connected = false;
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi disconnected. Retrying connection...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WiFi connected. Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_ip_address = event->ip_info.ip;
        s_is_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_manager_init_sta(void) {
    if (s_is_initialized) {
        ESP_LOGW(TAG, "WiFi manager already initialized.");
        return;
    }
    ESP_LOGI(TAG, "Initializing WiFi in STA mode...");
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &s_instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &s_instance_got_ip));

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    s_is_initialized = true;
    ESP_LOGI(TAG, "wifi_manager_init_sta finished. Waiting for connection...");
}

void wifi_manager_deinit_sta(void) {
    if (!s_is_initialized) {
        ESP_LOGW(TAG, "WiFi manager not initialized, cannot de-init.");
        return;
    }
    ESP_LOGI(TAG, "De-initializing WiFi in STA mode...");
    
    // Anular registro de eventos
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_instance_any_id);

    // Detener y desinicializar WiFi
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        // Ignorar este error, ya que es el estado que buscamos
    } else {
        ESP_ERROR_CHECK(err);
    }
    ESP_ERROR_CHECK(esp_wifi_deinit());
    
    // Destruir objetos de red
    // --- CORRECCIÓN AQUÍ ---
    esp_netif_destroy_default_wifi(s_sta_netif);
    // --- FIN DE LA CORRECCIÓN ---
    s_sta_netif = NULL;
    
    // Eliminar el bucle de eventos puede causar problemas si otros componentes lo usan.
    // Es más seguro dejarlo. Se puede descomentar si se está seguro de que no hay más usuarios.
    // ESP_ERROR_CHECK(esp_event_loop_delete_default());

    vEventGroupDelete(s_wifi_event_group);
    
    // Restablecer estado
    s_is_connected = false;
    s_is_initialized = false;
    ESP_LOGI(TAG, "WiFi de-initialized successfully.");
}


bool wifi_manager_is_connected(void) {
    return s_is_connected;
}

bool wifi_manager_get_ip_address(char* buffer, size_t buffer_size) {
    if (s_is_connected && buffer && buffer_size >= 16) {
        sprintf(buffer, IPSTR, IP2STR(&s_ip_address));
        return true;
    }
    return false;
}