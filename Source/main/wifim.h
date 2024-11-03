/*
 * Copyright (C) 2024 HTotoo
 *
 * This file is part of ESP32-Portapack.
 *
 * For additional license information, see the LICENSE file.
 */

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mdns.h"

#define CONFIG_AP_MAX_STA_CONN 4

#define WIFI_CLIENR_RC_TIME 35000

char wifiAPSSID[64] = {0};
char wifiAPPASS[64] = {0};

char wifiStaSSID[64] = {0};
char wifiStaPASS[64] = {0};

char wifiHostName[64] = {0};

int ap_client_num = 0;

uint32_t last_wifi_conntry = 0;
bool wifi_sta_ok = false;

extern "C"
{

  bool getWifiStaStatus()
  {
    return wifi_sta_ok;
  }

  int getWifiApClientNum()
  {
    return ap_client_num;
  }

  void initialise_mdns(void)
  {
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(wifiHostName));
    ESP_ERROR_CHECK(mdns_instance_name_set(wifiHostName));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_subtype_add_for_host(NULL, "_http", "_tcp", NULL, "_server"));
  }

  static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
  {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
      ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
      if (ap_client_num <= 0)
      {
        wifi_sta_ok = false;
        // esp_wifi_connect(); // only when no ap clients presents
      }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
      // count ap client number
      ap_client_num++;
      ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
      // count ap client number
      ap_client_num--;
      ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
      if (ap_client_num <= 0)
      {
        last_wifi_conntry = 0; // do it now!
      }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
      wifi_sta_ok = true;
      ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
    }
  }

  static void initialise_wifi(void)
  {
    static bool initialized = false;
    if (initialized)
    {
      return;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());
    initialise_mdns();
    initialized = true;
  }

  static bool wifi_apsta()
  {
    wifi_config_t ap_config = {};
    strcpy((char *)ap_config.ap.ssid, wifiAPSSID);
    strcpy((char *)ap_config.ap.password, wifiAPPASS);
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap_config.ap.ssid_len = strlen(wifiAPSSID);
    ap_config.ap.max_connection = CONFIG_AP_MAX_STA_CONN;
    ap_config.ap.channel = 0; // CONFIG_AP_WIFI_CHANNEL;

    if (strlen(wifiAPPASS) == 0)
    {
      ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config_t sta_config = {};
    strcpy((char *)sta_config.sta.ssid, wifiStaSSID);
    strcpy((char *)sta_config.sta.password, wifiStaPASS);
    sta_config.sta.failure_retry_cnt = 1;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WIFI_MODE_AP started. SSID:%s password:%s", wifiAPSSID, wifiAPPASS);

    ESP_ERROR_CHECK(esp_wifi_connect());

    return true;
  }

  void wifi_loop(uint32_t millis)
  {
    if (ap_client_num > 0 || wifi_sta_ok)
      return;
    if (millis - last_wifi_conntry > WIFI_CLIENR_RC_TIME)
    {
      ESP_LOGI(TAG, "esp_wifi_connect started.");
      esp_wifi_connect();
      last_wifi_conntry = millis;
    }
  }
}