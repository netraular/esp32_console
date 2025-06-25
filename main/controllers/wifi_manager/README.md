# WiFi Manager

## Description
This component provides a simple, reusable interface to manage the WiFi connection in Station (STA) mode. It handles initialization, connection, and reconnection automatically.

## How to Use

1.  **Initialize the Manager:**
    Call this once at application startup. This requires NVS to be initialized first.
    ```cpp
    #include "controllers/wifi_manager/wifi_manager.h"

    wifi_manager_init_sta();
    ```

2.  **Check Connection Status:**
    Use this function to see if the device is connected and has an IP address.
    ```cpp
    if (wifi_manager_is_connected()) {
        // We have a network connection
    }
    ```

3.  **Get IP Address:**
    Retrieve the current IP address as a string.
    ```cpp
    char ip_buf[16];
    if (wifi_manager_get_ip_address(ip_buf, sizeof(ip_buf))) {
        printf("My IP is: %s\n", ip_buf);
    }
    ```