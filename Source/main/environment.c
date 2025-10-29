
#include "environment.h"
#include "sensordb.h"
#include <string.h>

bmp280_t dev_bmp280;
i2c_dev_t bh1750;
sht3x_t sht3x;
sht4x_t sht4x;

EnvironmentSensors environment_inited = Environment_none;
EnvironmentLightSensors environment_light_inited = Environment_light_none;

void init_environment(int sda, int scl) {
    if (sda < 0 || scl < 0) {
        ESP_LOGI("Environment", "I2C pins not set, skipping environment sensor init");
        return;
    }
    // init extras
    init_environment_light(sda, scl);

    // init temp hum pressure

    environment_inited = Environment_none;

    // BMP280
    bmp280_params_t params_bmp280;
    bmp280_init_default_params(&params_bmp280);
    memset(&dev_bmp280, 0, sizeof(bmp280_t));
    if (bmp280_init_desc(&dev_bmp280, getDevAddr(BMx280), 0, sda, scl) == ESP_OK && bmp280_init(&dev_bmp280, &params_bmp280) == ESP_OK) {
        if ((dev_bmp280.id == BME280_CHIP_ID)) {
            ESP_LOGI("Environment", "bme280 OK");
        } else {
            ESP_LOGI("Environment", "bmp280 OK");
        }
        environment_inited = environment_inited | ((dev_bmp280.id == BME280_CHIP_ID) ? Environment_bme280 : Environment_bmp280);
        return;
    } else {
        // deinit
        bmp280_free_desc(&dev_bmp280);
    }

    // sht3x
    memset(&sht3x, 0, sizeof(sht3x_t));
    if (sht3x_init_desc(&sht3x, getDevAddr(SHT3x), 0, sda, scl) == ESP_OK && sht3x_init(&sht3x) == ESP_OK) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        sht3x_start_measurement(&sht3x, SHT3X_PERIODIC_1MPS, SHT3X_HIGH);
        environment_inited = environment_inited | Environment_sht3x;
        ESP_LOGI("Environment", "sht3x OK");
    }
    // deinit if fail
    else {
        sht3x_free_desc(&sht3x);
    }

    // sht4x
    memset(&sht4x, 0, sizeof(sht4x_t));
    if (sht4x_init_desc(&sht4x, getDevAddr(SHT4x), 0, sda, scl) == ESP_OK && sht4x_init(&sht4x) == ESP_OK) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        sht4x_start_measurement(&sht4x);
        environment_inited = environment_inited | Environment_sht4x;
        ESP_LOGI("Environment", "sht4x OK");
    }
    // deinit if fail
    else {
        sht4x_free_desc(&sht4x);
    }

    // end of list
    if (environment_inited == Environment_none) {
        ESP_LOGI("Environment", "No compatible sensor found");
    }
}

void get_environment_meas(float* temperature, float* pressure, float* humidity) {
    // put worst sensors front, then proceed to better ones.
    // reason: bmp has pressure, but sht30 don't. so we read all data from it, and override others from a more precise sensor.
    if (((environment_inited & Environment_bme280) == Environment_bme280) || ((environment_inited & Environment_bmp280) == Environment_bmp280)) {
        bmp280_read_float(&dev_bmp280, temperature, pressure, humidity);
    }
    if ((environment_inited & Environment_sht3x) == Environment_sht3x) {
        sht3x_get_results(&sht3x, temperature, humidity);
    }
    if ((environment_inited & Environment_sht4x) == Environment_sht4x) {
        sht4x_get_results(&sht4x, temperature, humidity);
    }
}

void get_environment_light(uint16_t* light) {
    if ((environment_light_inited & Environment_light_bh1750) == Environment_light_bh1750) {
        bh1750_read(&bh1750, light);
    }
}

void init_environment_light(int sda, int scl) {
    if (sda < 0 || scl < 0) {
        ESP_LOGI("EnvironmentLight", "I2C pins not set, skipping environment light sensor init");
        return;
    }
    environment_light_inited = Environment_light_none;

    // bh1750
    memset(&bh1750, 0, sizeof(i2c_dev_t));
    if (bh1750_init_desc(&bh1750, getDevAddr(BH1750), 0, sda, scl) == ESP_OK && bh1750_setup(&bh1750, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH) == ESP_OK) {
        bh1750_power_on(&bh1750);
        ESP_LOGI("EnvironmentLight", "bh1750 OK");
        environment_light_inited = environment_light_inited | Environment_light_bh1750;
    } else {
        bh1750_free_desc(&bh1750);
    }
}

bool is_environment_sensor_present() {
    return environment_inited != Environment_none;
}

bool is_environment_light_sensor_present() {
    return environment_light_inited != Environment_light_none;
}