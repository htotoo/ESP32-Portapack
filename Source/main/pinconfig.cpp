#include <nvs.h>
#include <nvs_flash.h>
#include "pinconfig.h"
#include "esp_log.h"

void PinConfig::saveToNvs() {
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open("pin_config", NVS_READWRITE, &nvsHandle);
    if (err == ESP_OK) {
        // Core
        nvs_set_i32(nvsHandle, "led_rgb_pin", ledRgbPin);
        nvs_set_i32(nvsHandle, "gps_rx_pin", gpsRxPin);
        nvs_set_i32(nvsHandle, "i2c_sda_pin", i2cSdaPin);
        nvs_set_i32(nvsHandle, "i2c_scl_pin", i2cSclPin);
        nvs_set_i32(nvsHandle, "ir_rx_pin", irRxPin);
        nvs_set_i32(nvsHandle, "ir_tx_pin", irTxPin);
        nvs_set_i32(nvsHandle, "i2c_sda_sl", i2cSdaSlavePin);
        nvs_set_i32(nvsHandle, "i2c_scl_sl", i2cSclSlavePin);

        // SPI
        nvs_set_i32(nvsHandle, "spi_sck_pin", spiSckPin);
        nvs_set_i32(nvsHandle, "spi_miso_pin", spiMisoPin);
        nvs_set_i32(nvsHandle, "spi_mosi_pin", spiMosiPin);

        // LoRa
        nvs_set_i32(nvsHandle, "lora_chip", static_cast<int32_t>(loraChipType));
        nvs_set_i32(nvsHandle, "lora_nss", loraNssPin);
        nvs_set_i32(nvsHandle, "lora_rst", loraResetPin);
        nvs_set_i32(nvsHandle, "lora_dio0", loraDio0Pin);
        nvs_set_i32(nvsHandle, "lora_dio1", loraDio1Pin);

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
        // Core
        nvs_get_i32(nvsHandle, "led_rgb_pin", &ledRgbPin);
        nvs_get_i32(nvsHandle, "gps_rx_pin", &gpsRxPin);
        nvs_get_i32(nvsHandle, "i2c_sda_pin", &i2cSdaPin);
        nvs_get_i32(nvsHandle, "i2c_scl_pin", &i2cSclPin);
        nvs_get_i32(nvsHandle, "ir_rx_pin", &irRxPin);
        nvs_get_i32(nvsHandle, "ir_tx_pin", &irTxPin);
        nvs_get_i32(nvsHandle, "i2c_sda_sl", &i2cSdaSlavePin);
        nvs_get_i32(nvsHandle, "i2c_scl_sl", &i2cSclSlavePin);

        // SPI
        nvs_get_i32(nvsHandle, "spi_sck_pin", &spiSckPin);
        nvs_get_i32(nvsHandle, "spi_miso_pin", &spiMisoPin);
        nvs_get_i32(nvsHandle, "spi_mosi_pin", &spiMosiPin);

        // LoRa
        int32_t radioTypeVal = -1;
        nvs_get_i32(nvsHandle, "lora_chip", &radioTypeVal);
        loraChipType = static_cast<LoraRadioType>(radioTypeVal);

        nvs_get_i32(nvsHandle, "lora_nss", &loraNssPin);
        nvs_get_i32(nvsHandle, "lora_rst", &loraResetPin);
        nvs_get_i32(nvsHandle, "lora_dio0", &loraDio0Pin);
        nvs_get_i32(nvsHandle, "lora_dio1", &loraDio1Pin);

        nvs_close(nvsHandle);
    } else {
        ESP_LOGE("PinConfig", "Failed to open NVS for reading: %s", esp_err_to_name(err));
        if (isPinsOk()) {
            saveToNvs();  // save current config as default
        }
    }
}

void PinConfig::debugPrint() {
    ESP_LOGI("PinConfig", "--- Current Pin Configuration ---");
    ESP_LOGI("PinConfig", "[Core]");
    ESP_LOGI("PinConfig", "  LED RGB: %ld", ledRgbPin);
    ESP_LOGI("PinConfig", "  GPS RX:  %ld", gpsRxPin);
    ESP_LOGI("PinConfig", "  I2C SDA: %ld", i2cSdaPin);
    ESP_LOGI("PinConfig", "  I2C SCL: %ld", i2cSclPin);
    ESP_LOGI("PinConfig", "  IR RX:   %ld", irRxPin);
    ESP_LOGI("PinConfig", "  IR TX:   %ld", irTxPin);
    ESP_LOGI("PinConfig", "  Slv SDA: %ld", i2cSdaSlavePin);
    ESP_LOGI("PinConfig", "  Slv SCL: %ld", i2cSclSlavePin);

    ESP_LOGI("PinConfig", "[SPI]");
    ESP_LOGI("PinConfig", "  SCK:  %ld", spiSckPin);
    ESP_LOGI("PinConfig", "  MISO: %ld", spiMisoPin);
    ESP_LOGI("PinConfig", "  MOSI: %ld", spiMosiPin);

    ESP_LOGI("PinConfig", "[LoRa]");
    ESP_LOGI("PinConfig", "  Chip: %d", static_cast<int>(loraChipType));
    ESP_LOGI("PinConfig", "  NSS:  %ld", loraNssPin);
    ESP_LOGI("PinConfig", "  RST:  %ld", loraResetPin);
    ESP_LOGI("PinConfig", "  DIO0: %ld", loraDio0Pin);
    ESP_LOGI("PinConfig", "  DIO1: %ld", loraDio1Pin);
    ESP_LOGI("PinConfig", "---------------------------------");
}