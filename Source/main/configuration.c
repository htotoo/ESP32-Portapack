#include "configuration.h"

void load_config_wifi()
{
    ESP_LOGI("CONFIG", "load_config_wifi");
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs_handle); // https://github.com/espressif/esp-idf/blob/v5.1.2/examples/storage/nvs_rw_value/main/nvs_value_example_main.c
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        strcpy(wifiAPSSID, DEFAULT_WIFI_AP);
        strcpy(wifiAPPASS, DEFAULT_WIFI_PASS);
        strcpy(wifiStaSSID, DEFAULT_WIFI_STA);
        strcpy(wifiStaPASS, DEFAULT_WIFI_PASS);
        strcpy(wifiHostName, DEFAULT_WIFI_HOSTNAME);
    }
    else
    {
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
        if (strlen(wifiHostName) < 1)
        {
            strcpy(wifiHostName, DEFAULT_WIFI_HOSTNAME);
        }
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI("CONFIG", "load_config_wifi ok");
    }
}

void save_config_wifi()
{
    ESP_LOGI("CONFIG", "save_config_wifi");
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK)
    {
        nvs_set_str(nvs_handle, "wifiHostName", (const char *)wifiHostName);
        nvs_set_str(nvs_handle, "wifiAPSSID", (const char *)wifiAPSSID);
        nvs_set_str(nvs_handle, "wifiAPPASS", (const char *)wifiAPPASS);
        nvs_set_str(nvs_handle, "wifiStaSSID", (const char *)wifiStaSSID);
        nvs_set_str(nvs_handle, "wifiStaPASS", (const char *)wifiStaPASS);
        nvs_close(nvs_handle);
        ESP_LOGI("CONFIG", "save_config_wifi ok");
    }
}

void load_config_orientation()
{
    ESP_LOGI("CONFIG", "load_config_orientation");
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("orient", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK)
    {
        nvs_get_i16(nvs_handle, "orientationXMin", &orientationXMin);
        nvs_get_i16(nvs_handle, "orientationYMin", &orientationYMin);
        nvs_get_i16(nvs_handle, "orientationZMin", &orientationZMin);
        nvs_get_i16(nvs_handle, "orientationXMax", &orientationXMax);
        nvs_get_i16(nvs_handle, "orientationYMax", &orientationYMax);
        nvs_get_i16(nvs_handle, "orientationZMax", &orientationZMax);
        int16_t tmp = 0;
        nvs_get_i16(nvs_handle, "declination", &tmp);
        declinationAngle = tmp / 100; // 2 decimal precision
        nvs_close(nvs_handle);
        ESP_LOGI("CONFIG", "load_config_orientation ok");
    }
    else
    {
        reset_orientation_calibration();
    }
}

void save_config_orientation()
{
    ESP_LOGI("CONFIG", "save_config_orientation");
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("orient", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK)
    {
        nvs_set_i16(nvs_handle, "orientationXMin", orientationXMin);
        nvs_set_i16(nvs_handle, "orientationYMin", orientationYMin);
        nvs_set_i16(nvs_handle, "orientationZMin", orientationZMin);
        nvs_set_i16(nvs_handle, "orientationXMax", orientationXMax);
        nvs_set_i16(nvs_handle, "orientationYMax", orientationYMax);
        nvs_set_i16(nvs_handle, "orientationZMax", orientationZMax);
        nvs_set_i16(nvs_handle, "declination", (int16_t)(declinationAngle * 100));
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI("CONFIG", "save_config_orientation ok");
    }
}

void reset_orientation_calibration()
{
    orientationXMin = INT16_MAX;
    orientationYMin = INT16_MAX;
    orientationZMin = INT16_MAX;
    orientationXMax = INT16_MIN;
    orientationYMax = INT16_MIN;
    orientationZMax = INT16_MIN;
}

void load_config_misc()
{
    ESP_LOGI("CONFIG", "load_config_misc");
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("misc", NVS_READWRITE, &nvs_handle); // https://github.com/espressif/esp-idf/blob/v5.1.2/examples/storage/nvs_rw_value/main/nvs_value_example_main.c
    rgb_brightness = 50;
    gps_baud = 9600;
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        nvs_get_u8(nvs_handle, "rgb_brightness", &rgb_brightness);
        nvs_get_u32(nvs_handle, "gps_baud", &gps_baud);
        nvs_close(nvs_handle);
        ESP_LOGI("CONFIG", "load_config_misc ok");
    }
}

void save_config_misc()
{
    ESP_LOGI("CONFIG", "save_config_misc");
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("misc", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK)
    {
        nvs_set_u8(nvs_handle, "rgb_brightness", rgb_brightness);
        nvs_set_u32(nvs_handle, "gps_baud", gps_baud);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI("CONFIG", "save_config_misc ok");
    }
    else
    {
        ESP_LOGI("CONFIG", "save_config_misc err: %d", err);
    }
}