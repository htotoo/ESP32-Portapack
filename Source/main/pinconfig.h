#pragma once

#include <cstdint>

// Enum for LoRa Radio Type
enum class LoraRadioType : int32_t {
    NONE = -1,
    SX1262 = 0,
    SX1261 = 1,
    SX1268 = 2,
    SX1276 = 3,
    LR1121 = 4,
    SX1278 = 5,
};

// Macros: [RGB, GPS, SDA, SCL, IR_RX, IR_TX, SDA_SL, SCL_SL, SCK, MISO, MOSI, RADIO_TYPE, NSS, RST, DIO0, DIO1]

#define HW_VARIANT_CUSTOM -1, 256, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1

#define HW_VARIANT_ESP32PP 48, 6, 5, 4, 12, 13, 11, 10, 35, 36, 37, -1, 38, 39, 40, 41
#define HW_VARIANT_MDK_BOARD -1, 4, 11, 10, 12, 13, 5, 6, 35, 36, 37, 5, 38, 39, 40, 41
#define HW_VARIANT_PRFAI -1, 256, 11, 10, 12, 13, 1, 2, 35, 36, 37, -1, 38, 39, 40, 41

class PinConfig {
   public:
    PinConfig() {};
    /**
     * @brief Construct a new Pin Config object.
     */
    PinConfig(int32_t ledRgb, int32_t gpsRx, int32_t i2cSda, int32_t i2cScl, int32_t irRx, int32_t irTx, int32_t i2cSdaSlave, int32_t i2cSclSlave, int32_t spiSck, int32_t spiMiso, int32_t spiMosi, int32_t radioType, int32_t loraNss, int32_t loraReset, int32_t loraDio0, int32_t loraDio1) {
        ledRgbPin = ledRgb;
        gpsRxPin = gpsRx;
        i2cSdaPin = i2cSda;
        i2cSclPin = i2cScl;
        irRxPin = irRx;
        irTxPin = irTx;
        i2cSdaSlavePin = i2cSdaSlave;
        i2cSclSlavePin = i2cSclSlave;

        // SPI
        spiSckPin = spiSck;
        spiMisoPin = spiMiso;
        spiMosiPin = spiMosi;

        // LoRa
        loraChipType = static_cast<LoraRadioType>(radioType);
        loraNssPin = loraNss;
        loraResetPin = loraReset;
        loraDio0Pin = loraDio0;
        loraDio1Pin = loraDio1;
    };

    void setPins(int32_t ledRgb, int32_t gpsRx, int32_t i2cSda, int32_t i2cScl, int32_t irRx, int32_t irTx, int32_t i2cSdaSlave, int32_t i2cSclSlave, int32_t spiSck, int32_t spiMiso, int32_t spiMosi, int32_t radioType, int32_t loraNss, int32_t loraReset, int32_t loraDio0, int32_t loraDio1) {
        ledRgbPin = ledRgb;
        gpsRxPin = gpsRx;
        i2cSdaPin = i2cSda;
        i2cSclPin = i2cScl;
        irRxPin = irRx;
        irTxPin = irTx;
        i2cSdaSlavePin = i2cSdaSlave;
        i2cSclSlavePin = i2cSclSlave;

        spiSckPin = spiSck;
        spiMisoPin = spiMiso;
        spiMosiPin = spiMosi;

        loraChipType = static_cast<LoraRadioType>(radioType);
        loraNssPin = loraNss;
        loraResetPin = loraReset;
        loraDio0Pin = loraDio0;
        loraDio1Pin = loraDio1;
    };

    bool isPinsOk() { return (i2cSdaSlavePin != -1 && i2cSclSlavePin != -1); }  // the bare minimum

    // Core Getters
    int32_t LedRgbPin() { return ledRgbPin; }
    int32_t GpsRxPin() { return gpsRxPin; }
    int32_t I2cSdaPin() { return i2cSdaPin; }
    int32_t I2cSclPin() { return i2cSclPin; }
    int32_t IrRxPin() { return irRxPin; }
    int32_t IrTxPin() { return irTxPin; }
    int32_t I2cSdaSlavePin() { return i2cSdaSlavePin; }
    int32_t I2cSclSlavePin() { return i2cSclSlavePin; }

    // SPI Getters
    int32_t SpiSckPin() { return spiSckPin; }
    int32_t SpiMisoPin() { return spiMisoPin; }
    int32_t SpiMosiPin() { return spiMosiPin; }

    // LoRa Getters
    LoraRadioType LoraChipType() { return loraChipType; }
    int32_t LoraNssPin() { return loraNssPin; }
    int32_t LoraResetPin() { return loraResetPin; }
    int32_t LoraDio0Pin() { return loraDio0Pin; }
    int32_t LoraDio1Pin() { return loraDio1Pin; }

    void saveToNvs();    // save current config to nvs
    void loadFromNvs();  // load config from nvs

    void debugPrint();  // print current config to log

    bool hasGPS() { return (gpsRxPin < 200); }
    bool hasIRrx() { return (irRxPin != -1); }
    bool hasIRtx() { return (irTxPin != -1); }
    bool hasSPI() { return (spiSckPin != -1 && spiMisoPin != -1 && spiMosiPin != -1); }
    bool hasLoRa() { return (loraChipType != LoraRadioType::NONE && loraNssPin != -1); }

   protected:
    // Core
    int32_t ledRgbPin = -1;
    int32_t gpsRxPin = 256;

    int32_t i2cSdaPin = -1;
    int32_t i2cSclPin = -1;

    int32_t irRxPin = -1;
    int32_t irTxPin = -1;

    int32_t i2cSdaSlavePin = -1;
    int32_t i2cSclSlavePin = -1;

    // SPI Master
    int32_t spiSckPin = -1;
    int32_t spiMisoPin = -1;
    int32_t spiMosiPin = -1;

    // LoRa Radio
    LoraRadioType loraChipType = LoraRadioType::NONE;
    int32_t loraNssPin = -1;
    int32_t loraResetPin = -1;
    int32_t loraDio0Pin = -1;
    int32_t loraDio1Pin = -1;
};