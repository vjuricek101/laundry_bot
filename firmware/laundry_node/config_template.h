#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi Configuration
const char *WIFI_SSID = "your_wifi_ssid";
const char *WIFI_PASSWORD = "your_wifi_password";

// MQTT Configuration (Synthetic Data)
const char *MQTT_BROKER_IP = "123";
const int MQTT_BROKER_PORT = 1234;
const char *MQTT_USERNAME = "laundry_user";
const char *MQTT_PASSWORD = "laundry_password";

// Node Configuration
const char *BUILDING_ID = "roble";
const char *MACHINE_ID = "synth_w01";
const char *MACHINE_TYPE = "washer"; // "washer" or "dryer" — set per deployment

#endif // CONFIG_H
