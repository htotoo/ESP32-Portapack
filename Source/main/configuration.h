#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "nvs_flash.h"
#include "string.h"
#include "esp_log.h"

// no need to change, just connect to the AP, and change in the settings.
#define DEFAULT_WIFI_HOSTNAME "ESP32PP"
#define DEFAULT_WIFI_AP "ESP32PP"
#define DEFAULT_WIFI_PASS "12345678"
#define DEFAULT_WIFI_STA "Hotspot"

// wifi config variables
extern char wifiAPSSID[64];
extern char wifiAPPASS[64];

extern char wifiStaSSID[64];
extern char wifiStaPASS[64];

extern char wifiHostName[64];

// orientation variables

extern int16_t orientationXMin;
extern int16_t orientationYMin;
extern int16_t orientationZMin;
extern int16_t orientationXMax;
extern int16_t orientationYMax;
extern int16_t orientationZMax;

extern float declinationAngle;

// misc
extern uint8_t rgb_brightness;
extern uint32_t gps_baud;

#if __cplusplus
extern "C"
{
#endif

void load_config_wifi();
void save_config_wifi();
void load_config_orientation();
void save_config_orientation();
void reset_orientation_calibration();
void load_config_misc(); // led brightness
void save_config_misc();

#if __cplusplus
}
#endif

#endif