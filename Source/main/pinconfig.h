#pragma once

#include <cstdint>

#define HW_VARIANT_CUSTOM -1, 256, -1, -1, -1, -1, -1, -1
#define HW_VARIANT_ESP32PP 48, 6, 5, 4, 12, 13, 11, 10
#define HW_VARIANT_MDK_BOARD -1, 4, 11, 10, 12, 13, 5, 6
#define HW_VARIANT_PRFAI -1, 256, 11, 10, 12, 13, 1, 2

class PinConfig {
   public:
    PinConfig() {};
    /**
     * @brief Construct a new Pin Config object from a fixed list of pin numbers.
     * This is highly efficient for embedded systems as it involves no templates or dynamic allocation.
     * @param ledRgb Pin for the RGB LED.
     * @param gpsRx Pin for the GPS.
     * @param i2cSda Pin for the I2C SDA (Master).
     * @param i2cScl Pin for the I2C SCL (Master).
     * @param irRx Pin for the IR Receiver.
     * @param irTx Pin for the IR Transmitter.
     * @param i2cSdaSlave Pin for the I2C SDA (Slave).
     * @param i2cSclSlave Pin for the I2C SCL (Slave).
     */
    PinConfig(int32_t ledRgb, int32_t gpsRx, int32_t i2cSda, int32_t i2cScl, int32_t irRx, int32_t irTx, int32_t i2cSdaSlave, int32_t i2cSclSlave) {
        ledRgbPin = ledRgb;
        gpsRxPin = gpsRx;
        i2cSdaPin = i2cSda;
        i2cSclPin = i2cScl;
        irRxPin = irRx;
        irTxPin = irTx;
        i2cSdaSlavePin = i2cSdaSlave;
        i2cSclSlavePin = i2cSclSlave;
    };

    void setPins(int32_t ledRgb, int32_t gpsRx, int32_t i2cSda, int32_t i2cScl, int32_t irRx, int32_t irTx, int32_t i2cSdaSlave, int32_t i2cSclSlave) {
        ledRgbPin = ledRgb;
        gpsRxPin = gpsRx;
        i2cSdaPin = i2cSda;
        i2cSclPin = i2cScl;
        irRxPin = irRx;
        irTxPin = irTx;
        i2cSdaSlavePin = i2cSdaSlave;
        i2cSclSlavePin = i2cSclSlave;
    };
    bool isPinsOk() { return (i2cSdaSlavePin != -1 && i2cSclSlavePin != -1); }  // the bare minimum

    int32_t LedRgbPin() { return ledRgbPin; }
    int32_t GpsRxPin() { return gpsRxPin; }
    int32_t I2cSdaPin() { return i2cSdaPin; }
    int32_t I2cSclPin() { return i2cSclPin; }
    int32_t IrRxPin() { return irRxPin; }
    int32_t IrTxPin() { return irTxPin; }
    int32_t I2cSdaSlavePin() { return i2cSdaSlavePin; }
    int32_t I2cSclSlavePin() { return i2cSclSlavePin; }

    void saveToNvs();    // save current config to nvs
    void loadFromNvs();  // load config from nvs

    void debugPrint();  // print current config to log

    bool hasGPS() { return (gpsRxPin < 200); }
    bool hasIRrx() { return (irRxPin != -1); }
    bool hasIRtx() { return (irTxPin != -1); }

   protected:
    int32_t ledRgbPin = -1;  // -1 = not used. this is the rgb led pin. single pin, that uses ledstrip_controller
    int32_t gpsRxPin = 256;  // 256 = not used this it the uart rx port of the esp, where the gps's tx pin is wired.

    int32_t i2cSdaPin = -1;  // -1 = not used this is the i2c sda pin (master). you'll wire the sda of the sensors here.
    int32_t i2cSclPin = -1;  // -1 = not used this is the i2c scl pin (master). you'll wire the scl of the sensors here.

    int32_t irRxPin = -1;  // -1 = not used this is the ir receiver pin.
    int32_t irTxPin = -1;  // -1 = not used this is the ir transmitter pin.

    int32_t i2cSdaSlavePin = -1;  // -1 = not used this is the i2c sda pin (slave). this will be connected to portapack's i2c master sda pin.
    int32_t i2cSclSlavePin = -1;  // -1 = not used this is the i2c scl pin (slave). this will be connected to portapack's i2c master scl pin.
};