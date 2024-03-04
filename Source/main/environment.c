
#include "environment.h"

#include <string.h>

bmp280_t dev_bmp280;

EnvironmentSensors environment_inited = Environment_none;

void init_environment()
{
    // BMP280
    bmp280_params_t params_bmp280;
    bmp280_init_default_params(&params_bmp280);
    memset(&dev_bmp280, 0, sizeof(bmp280_t));
    bmp280_init_desc(&dev_bmp280, BMP280_I2C_ADDRESS_0, 0, 5, 4);

    if (bmp280_init(&dev_bmp280, &params_bmp280) == ESP_OK)
    {
        if ((dev_bmp280.id == BME280_CHIP_ID))
        {
            ESP_LOGI("Environment", "bme280 OK");
        }
        else
        {
            ESP_LOGI("Environment", "bmp280 OK");
        }
        environment_inited = (dev_bmp280.id == BME280_CHIP_ID) ? Environment_bme280 : Environment_bmp280;
    }
    else
    {
        // deinit
        bmp280_free_desc(&dev_bmp280);
    }

    // end of list
    if (environment_inited == Environment_none)
    {
        ESP_LOGI("Environment", "No compatible sensor found");
    }
}

void get_environment_meas(float *temperature, float *pressure, float *humidity)
{
    if (environment_inited == Environment_bme280 || environment_inited == Environment_bmp280)
    {
        bmp280_read_float(&dev_bmp280, temperature, pressure, humidity);
        return;
    }
}