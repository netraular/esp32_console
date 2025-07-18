#include "wifi_manager.h"
#include "secret.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include <string.h>
#include <time.h>

static const char *TAG = "WIFI_MGR";

static EventGroupHandle_t s_wifi_event_group;
// These are defined as extern in the header, so they must be global (not static) here.
const int WIFI_CONNECTED_BIT = BIT0;
const int TIME_SYNC_BIT = BIT1;

static esp_ip4_addr_t s_ip_address;
static bool s_is_connected = false;
static bool s_is_initialized = false; 

static esp_event_handler_instance_t s_instance_any_id;
static esp_event_handler_instance_t s_instance_got_ip;
static esp_netif_t *s_sta_netif = NULL;

static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Time synchronized successfully");
    char strftime_buf[64];
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Current time: %s", strftime_buf);
    xEventGroupSetBits(s_wifi_event_group, TIME_SYNC_BIT);
}

static void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_is_connected = false;
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, TIME_SYNC_BIT);
        ESP_LOGI(TAG, "WiFi disconnected. Retrying connection...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WiFi connected. Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_ip_address = event->ip_info.ip;
        s_is_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        
        if (esp_sntp_enabled() == 0) {
             initialize_sntp();
        }
    }
}

void wifi_manager_init_sta(void) {
    if (s_is_initialized) {
        ESP_LOGW(TAG, "WiFi manager already initialized.");
        if (!s_is_connected) {
             ESP_LOGI(TAG, "Already initialized but not connected. Attempting to connect again.");
             esp_wifi_connect();
        }
        return;
    }
    ESP_LOGI(TAG, "Initializing WiFi in STA mode...");
    s_wifi_event_group = xEventGroupCreate();

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
    
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }
    
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_instance_any_id);

    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        // This is fine, means it was already stopped or not running.
    } else {
        ESP_ERROR_CHECK(err);
    }
    ESP_ERROR_CHECK(esp_wifi_deinit());
    
    esp_netif_destroy_default_wifi(s_sta_netif);
    s_sta_netif = NULL;
    
    vEventGroupDelete(s_wifi_event_group);
    
    s_is_connected = false;
    s_is_initialized = false;
    ESP_LOGI(TAG, "WiFi de-initialized successfully.");
}

bool wifi_manager_is_connected(void) {
    if (!s_wifi_event_group) {
        return false;
    }
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    // For full network readiness, we require both connection and time sync.
    return (bits & (WIFI_CONNECTED_BIT | TIME_SYNC_BIT)) == (WIFI_CONNECTED_BIT | TIME_SYNC_BIT);
}

bool wifi_manager_get_ip_address(char* buffer, size_t buffer_size) {
    if (s_is_connected && buffer && buffer_size >= 16) {
        sprintf(buffer, IPSTR, IP2STR(&s_ip_address));
        return true;
    }
    return false;
}

// --- FIX: Implement the function declared in the header ---
EventGroupHandle_t wifi_manager_get_event_group(void) {
    return s_wifi_event_group;
}