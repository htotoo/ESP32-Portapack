#include "configuration.h"
#include "led.h"

#if __cplusplus
extern "C"
{
#endif

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
        uint8_t rgb_brightness = 50;

        gps_baud = 9600;
        if (err != ESP_OK)
        {
            printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        }
        else
        {
            nvs_get_u8(nvs_handle, "rgb_brightness", &rgb_brightness);
            LedFeedback::set_brightness(rgb_brightness);
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
            nvs_set_u8(nvs_handle, "rgb_brightness", LedFeedback::get_brightness());
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

#if __cplusplus
}
#endif