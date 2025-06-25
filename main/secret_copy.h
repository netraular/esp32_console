#ifndef CONFIG_H_
#define CONFIG_H_

/******************************************************************************
 * Example Configuration File for the ESP32 Audio Streamer
 *
 * --- INSTRUCTIONS ---
 * 1. Duplicate this file.
 * 2. Rename the copy to "config.h".
 * 3. Fill in your actual WiFi and server details in "config.h".
 *
 * The "config.h" file is ignored by Git, so your credentials will remain private.
 ******************************************************************************/

// --- WiFi Configuration ---
// Replace with your WiFi network name (SSID)
#define WIFI_SSID      "Your_WiFi_SSID"

// Replace with your WiFi password
#define WIFI_PASS      "Your_WiFi_Password"


// --- TCP Server Configuration ---
// Replace with the IP address of the PC running the server
#define HOST_IP_ADDR "Your_PC_IP_Address"

// The server port (default is 8888, change only if necessary)
#define HOST_PORT    8888


#endif // CONFIG_H_