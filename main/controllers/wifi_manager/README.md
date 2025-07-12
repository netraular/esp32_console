# WiFi Manager

## Description
This component provides a simple, reusable interface to manage the WiFi connection in Station (STA) mode. It handles initialization, connection, automatic reconnection, and SNTP time synchronization.

## Features
-   **Automatic Connection:** Connects to the WiFi credentials specified in `secret.h`.
-   **Auto-Reconnect:** Automatically attempts to reconnect if the connection is lost.
-   **SNTP Time Sync:** Initializes and synchronizes the system time from an NTP server after connecting.
-   **Event Group:** Provides an RTOS event group to allow other tasks to efficiently wait for network readiness (`WIFI_CONNECTED_BIT`) and time synchronization (`TIME_SYNC_BIT`).
-   **Simple API:** Offers straightforward functions to check status and get the IP address.

## How to Use

1.  **Configure Credentials:**
    Add your WiFi SSID and password to `secret.h`.
    ```c
    #define WIFI_SSID "Your_WiFi_SSID"
    #define WIFI_PASS "Your_WiFi_Password"
    ```

2.  **Initialize the Manager:**
    Call this once at application startup. This requires NVS and the default event loop to be initialized first.
    ```cpp
    #include "controllers/wifi_manager/wifi_manager.h"

    wifi_manager_init_sta();
    ```

3.  **Check Connection Status:**
    Use this function to see if the device has a full network connection (IP and time).
    ```cpp
    if (wifi_manager_is_connected()) {
        // We are ready for network operations
    }
    ```

4.  **Get IP Address:**
    Retrieve the current IP address as a string.
    ```cpp
    char ip_buf[16];
    if (wifi_manager_get_ip_address(ip_buf, sizeof(ip_buf))) {
        printf("My IP is: %s\n", ip_buf);
    }
    ```

5.  **Wait for Connection in a Task:**
    Other modules can use the event group to wait for the connection.
    ```cpp
    #include "controllers/wifi_manager/wifi_manager.h"
    
    EventGroupHandle_t wifi_events = wifi_manager_get_event_group();
    xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT | TIME_SYNC_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    // ...proceed with network task...
    ```
    
## Dependencies
-   `esp_wifi`, `esp_netif`, `esp_event`, `esp_sntp`.
-   Requires NVS and the default event loop to be initialized.
-   `secret.h` for WiFi credentials.