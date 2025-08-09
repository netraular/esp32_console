#ifndef SECRETS_H_
#define SECRETS_H_

/******************************************************************************
 * Secrets Configuration File Template
 *
 * --- INSTRUCTIONS ---
 * 1. Duplicate this file in the same directory.
 * 2. Rename the copy to "secrets.h".
 * 3. Fill in your actual credentials in the "secrets.h" file.
 *
 * The "secrets.h" file is ignored by Git, so your credentials will remain private.
 ******************************************************************************/

// --- WiFi Configuration ---
// Replace with your WiFi network name (SSID)
#define WIFI_SSID      "Your_WiFi_SSID"

// Replace with your WiFi password
#define WIFI_PASS      "Your_WiFi_Password"


// --- TCP Server Configuration ---
// Replace with the IP address of the PC running the server
#define STREAMING_SERVER_IP "Your_PC_IP_Address"

// The server port (default is 8888, change only if necessary)
#define STREAMING_SERVER_PORT    8888

// --- Groq API Configuration ---
// Replace with your Groq API Key for Speech-to-Text
#define GROQ_API_KEY   "Your_Groq_API_Key"

#endif // SECRETS_H_