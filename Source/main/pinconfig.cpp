#include <nvs.h>
#include <nvs_flash.h>
#include "pinconfig.h"
#include "esp_log.h"

void PinConfig::saveToNvs() {
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open("pin_config", NVS_READWRITE, &nvsHandle);
    if (err == ESP_OK) {
        nvs_set_i32(nvsHandle, "led_rgb_pin", ledRgbPin);
        nvs_set_i32(nvsHandle, "gps_rx_pin", gpsRxPin);
        nvs_set_i32(nvsHandle, "i2c_sda_pin", i2cSdaPin);
        nvs_set_i32(nvsHandle, "i2c_scl_pin", i2cSclPin);
        nvs_set_i32(nvsHandle, "ir_rx_pin", irRxPin);
        nvs_set_i32(nvsHandle, "ir_tx_pin", irTxPin);
        nvs_set_i32(nvsHandle, "i2c_sda_sl", i2cSdaSlavePin);
        nvs_set_i32(nvsHandle, "i2c_scl_sl", i2cSclSlavePin);
        nvs_commit(nvsHandle);
        nvs_close(nvsHandle);
    } else {
        ESP_LOGE("PinConfig", "Failed to open NVS for writing: %s", esp_err_to_name(err));
    }
}

void PinConfig::loadFromNvs() {
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open("pin_config", NVS_READONLY, &nvsHandle);
    if (err == ESP_OK) {
        nvs_get_i32(nvsHandle, "led_rgb_pin", &ledRgbPin);
        nvs_get_i32(nvsHandle, "gps_rx_pin", &gpsRxPin);
        nvs_get_i32(nvsHandle, "i2c_sda_pin", &i2cSdaPin);
        nvs_get_i32(nvsHandle, "i2c_scl_pin", &i2cSclPin);
        nvs_get_i32(nvsHandle, "ir_rx_pin", &irRxPin);
        nvs_get_i32(nvsHandle, "ir_tx_pin", &irTxPin);
        nvs_get_i32(nvsHandle, "i2c_sda_sl", &i2cSdaSlavePin);
        nvs_get_i32(nvsHandle, "i2c_scl_sl", &i2cSclSlavePin);
        nvs_close(nvsHandle);
    } else {
        ESP_LOGE("PinConfig", "Failed to open NVS for reading: %s", esp_err_to_name(err));
    }
}

void PinConfig::debugPrint() {
    ESP_LOGI("PinConfig", "Current Pin Configuration:");
    ESP_LOGI("PinConfig", "LED RGB Pin: %d", ledRgbPin);
    ESP_LOGI("PinConfig", "GPS RX Pin: %d", gpsRxPin);
    ESP_LOGI("PinConfig", "I2C SDA Pin: %d", i2cSdaPin);
    ESP_LOGI("PinConfig", "I2C SCL Pin: %d", i2cSclPin);
    ESP_LOGI("PinConfig", "IR RX Pin: %d", irRxPin);
    ESP_LOGI("PinConfig", "IR TX Pin: %d", irTxPin);
    ESP_LOGI("PinConfig", "I2C SDA Slave Pin: %d", i2cSdaSlavePin);
    ESP_LOGI("PinConfig", "I2C SCL Slave Pin: %d", i2cSclSlavePin);
}