#pragma once

#include <cstdint>

class PinConfig {
   public:
    bool isPinsOk() { return true; }

    int LedRgbPin() { return ledRgbPin; }
    int GpsRxPin() { return gpsRxPin; }
    int I2cSdaPin() { return i2cSdaPin; }
    int I2cSclPin() { return i2cSclPin; }
    int IrRxPin() { return irRxPin; }
    int IrTxPin() { return irTxPin; }

   protected:
    int ledRgbPin = -1;  // -1 = not used
    int gpsRxPin = 256;  // 256 = not used

    int i2cSdaPin = -1;  // -1 = not used
    int i2cSclPin = -1;  // -1 = not used

    int irRxPin = -1;  // -1 = not used
    int irTxPin = -1;  // -1 = not used
};