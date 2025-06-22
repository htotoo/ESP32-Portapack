/*
 * Copyright (C) 2024 HTotoo
 *
 * This file is part of ESP32-Portapack.
 *
 * For additional license information, see the LICENSE file.
 */

#ifndef WIFIM_H
#define WIFIM_H

// no need to change, just connect to the AP, and change in the settings.
#define DEFAULT_WIFI_HOSTNAME "ESP32PP"
#define DEFAULT_WIFI_AP "ESP32PP"
#define DEFAULT_WIFI_PASS "12345678"
#define DEFAULT_WIFI_STA "-"

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "string.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mdns.h"
#include <string>

#define CONFIG_AP_MAX_STA_CONN 4
#define WIFI_CLIENR_RC_TIME 35000

class WifiM {
   public:
    static bool getWifiStaStatus();
    static void wifi_loop(uint32_t millis);
    static bool wifi_apsta();
    static void initialise_wifi(void);
    static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void initialise_mdns(void);
    static int getWifiApClientNum();
    static void save_config_wifi();
    static void load_config_wifi();
    static std::string getStaIp();
    static std::string getApIp();

    static char wifiAPSSID[64];
    static char wifiAPPASS[64];
    static char wifiStaSSID[64];
    static char wifiStaPASS[64];
    static char wifiHostName[64];

   private:
    static int ap_client_num;

    static uint32_t last_wifi_conntry;
    static bool wifi_sta_ok;
};

#endif
