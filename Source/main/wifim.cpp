#include "wifim.h"

#define TAG "WIFIM"

char WifiM::wifiAPSSID[64] = {0};
char WifiM::wifiAPPASS[64] = {0};
char WifiM::wifiStaSSID[64] = {0};
char WifiM::wifiStaPASS[64] = {0};
char WifiM::wifiHostName[64] = {0};

int WifiM::ap_client_num = 0;

uint32_t WifiM::last_wifi_conntry = 0;
bool WifiM::wifi_sta_ok = false;

bool WifiM::getWifiStaStatus() {
    return wifi_sta_ok;
}

int WifiM::getWifiApClientNum() {
    return ap_client_num;
}

void WifiM::initialise_mdns(void) {
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(wifiHostName));
    ESP_ERROR_CHECK(mdns_instance_name_set(wifiHostName));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_subtype_add_for_host(NULL, "_http", "_tcp", NULL, "_server"));
}

void WifiM::event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
        if (ap_client_num <= 0) {
            wifi_sta_ok = false;
            // esp_wifi_connect(); // only when no ap clients presents
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        // count ap client number
        ap_client_num++;
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        // count ap client number
        ap_client_num--;
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
        if (ap_client_num <= 0) {
            last_wifi_conntry = 0;  // do it now!
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_sta_ok = true;
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
    }
}

void WifiM::initialise_wifi(void) {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
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

bool WifiM::wifi_apsta() {
    wifi_config_t ap_config = {};
    strcpy((char*)ap_config.ap.ssid, wifiAPSSID);
    strcpy((char*)ap_config.ap.password, wifiAPPASS);
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap_config.ap.ssid_len = strlen(wifiAPSSID);
    ap_config.ap.max_connection = CONFIG_AP_MAX_STA_CONN;
    ap_config.ap.channel = 0;  // CONFIG_AP_WIFI_CHANNEL;

    if (strlen(wifiAPPASS) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config_t sta_config = {};
    strcpy((char*)sta_config.sta.ssid, wifiStaSSID);
    strcpy((char*)sta_config.sta.password, wifiStaPASS);
    sta_config.sta.failure_retry_cnt = 1;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WIFI_MODE_AP started. SSID:%s password:%s", wifiAPSSID, wifiAPPASS);

    ESP_ERROR_CHECK(esp_wifi_connect());

    return true;
}

void WifiM::wifi_loop(uint32_t millis) {
    if (ap_client_num > 0 || wifi_sta_ok)
        return;
    if (millis - last_wifi_conntry > WIFI_CLIENR_RC_TIME) {
        ESP_LOGI(TAG, "esp_wifi_connect started.");
        esp_wifi_connect();
        last_wifi_conntry = millis;
    }
}

void WifiM::load_config_wifi() {
    ESP_LOGI("CONFIG", "load_config_wifi");
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);  // https://github.com/espressif/esp-idf/blob/v5.1.2/examples/storage/nvs_rw_value/main/nvs_value_example_main.c
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        strcpy(wifiAPSSID, DEFAULT_WIFI_AP);
        strcpy(wifiAPPASS, DEFAULT_WIFI_PASS);
        strcpy(wifiStaSSID, DEFAULT_WIFI_STA);
        strcpy(wifiStaPASS, DEFAULT_WIFI_PASS);
        strcpy(wifiHostName, DEFAULT_WIFI_HOSTNAME);
    } else {
        size_t size = sizeof(wifiAPSSID);
        nvs_get_str(nvs_handle, "wifiAPSSID", wifiAPSSID, &size);
        if (strlen(wifiAPSSID) < 1)
            strcpy(wifiAPSSID, DEFAULT_WIFI_AP);
        size = sizeof(wifiAPPASS);
        nvs_get_str(nvs_handle, "wifiAPPASS", wifiAPPASS, &size);
        if (strlen(wifiAPPASS) < 1)
            strcpy(wifiAPPASS, DEFAULT_WIFI_PASS);

        size = sizeof(wifiStaSSID);
        nvs_get_str(nvs_handle, "wifiStaSSID", wifiStaSSID, &size);
        if (strlen(wifiStaSSID) < 1)
            strcpy(wifiStaSSID, DEFAULT_WIFI_STA);
        size = sizeof(wifiStaPASS);
        nvs_get_str(nvs_handle, "wifiStaPASS", wifiStaPASS, &size);
        if (strlen(wifiStaPASS) < 1)
            strcpy(wifiStaPASS, DEFAULT_WIFI_PASS);
        size = sizeof(wifiHostName);
        nvs_get_str(nvs_handle, "wifiHostName", wifiHostName, &size);
        if (strlen(wifiHostName) < 1) {
            strcpy(wifiHostName, DEFAULT_WIFI_HOSTNAME);
        }
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI("CONFIG", "load_config_wifi ok");
    }
}

void WifiM::save_config_wifi() {
    ESP_LOGI("CONFIG", "save_config_wifi");
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_str(nvs_handle, "wifiHostName", (const char*)wifiHostName);
        nvs_set_str(nvs_handle, "wifiAPSSID", (const char*)wifiAPSSID);
        nvs_set_str(nvs_handle, "wifiAPPASS", (const char*)wifiAPPASS);
        nvs_set_str(nvs_handle, "wifiStaSSID", (const char*)wifiStaSSID);
        nvs_set_str(nvs_handle, "wifiStaPASS", (const char*)wifiStaPASS);
        nvs_close(nvs_handle);
        ESP_LOGI("CONFIG", "save_config_wifi ok");
    }
}
