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
     * @param gpsRx Pin for the GPS RX.
     * @param i2cSda Pin for the I2C SDA (Master).
     * @param i2cScl Pin for the I2C SCL (Master).
     * @param irRx Pin for the IR Receiver.
     * @param irTx Pin for the IR Transmitter.
     * @param i2cSdaSlave Pin for the I2C SDA (Slave).
     * @param i2cSclSlave Pin for the I2C SCL (Slave).
     */
    PinConfig(int ledRgb, int gpsRx, int i2cSda, int i2cScl, int irRx, int irTx, int i2cSdaSlave, int i2cSclSlave) {
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

    int LedRgbPin() { return ledRgbPin; }
    int GpsRxPin() { return gpsRxPin; }
    int I2cSdaPin() { return i2cSdaPin; }
    int I2cSclPin() { return i2cSclPin; }
    int IrRxPin() { return irRxPin; }
    int IrTxPin() { return irTxPin; }
    int I2cSdaSlavePin() { return i2cSdaSlavePin; }
    int I2cSclSlavePin() { return i2cSclSlavePin; }

   protected:
    int ledRgbPin = -1;  // -1 = not used
    int gpsRxPin = 256;  // 256 = not used

    int i2cSdaPin = -1;  // -1 = not used
    int i2cSclPin = -1;  // -1 = not used

    int irRxPin = -1;  // -1 = not used
    int irTxPin = -1;  // -1 = not used

    int i2cSdaSlavePin = -1;  // -1 = not used
    int i2cSclSlavePin = -1;  // -1 = not used
};