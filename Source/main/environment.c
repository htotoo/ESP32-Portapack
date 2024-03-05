
#include "environment.h"

#include <string.h>

bmp280_t dev_bmp280;
i2c_dev_t bh1750;

EnvironmentSensors environment_inited = Environment_none;
EnvironmentLightSensors environment_light_inited = Environment_light_none;

void init_environment()
{
    // init extras
    init_environment_light();
    // init temp hum pressure

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
        return;
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

void get_environment_light(uint16_t *light)
{
    if (environment_light_inited == Environment_light_bh1750)
    {
        bh1750_read(&bh1750, light);
        return;
    }
}

void init_environment_light()
{
    environment_light_inited = Environment_light_none;

    // bh1750
    memset(&bh1750, 0, sizeof(i2c_dev_t));
    bh1750_init_desc(&bh1750, 0x23, 0, 5, 4);

    if (bh1750_setup(&bh1750, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH) == ESP_OK)
    {
        bh1750_power_on(&bh1750);
        ESP_LOGI("EnvironmentLight", "bh1750 OK");
        environment_light_inited = Environment_light_bh1750;
        return;
    }
    else
    {
        bh1750_free_desc(&bh1750);
    }
}