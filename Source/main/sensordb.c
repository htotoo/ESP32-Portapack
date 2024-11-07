#include "sensordb.h"

uint8_t BH1750_addr = 0;
uint8_t HMC5883L_addr = 0;
uint8_t ADXL345_addr = 0;
uint8_t MPU925X_addr = 0;
uint8_t BMx280_addr = 0;
uint8_t SHT3x_addr = 0;
uint8_t LSM303_addr_mag = 0;
uint8_t LSM303_addr_acc = 0;

void foundI2CDev(uint8_t addr)
{
    if (addr == 0x23)
        BH1750_addr = 0x23; // low
    if (addr == 0x5c)
        BH1750_addr = 0x5c; // high
    if (addr == 0x76)
        BMx280_addr = 0x76; // low
    if (addr == 0x77)
        BMx280_addr = 0x77; // high
    if (addr == 0x44)
        SHT3x_addr = 0x44;
    if (addr == 0x45)
        SHT3x_addr = 0x45;
    if (addr == 0x53)
        ADXL345_addr = 0x53; // sdo 0
    if (addr == 0x1D)
        ADXL345_addr = 0x1D; // sdo 1

    if (addr == 0x1E)
        HMC5883L_addr = 0x1E;

    if (addr == 0x68)
        MPU925X_addr = 0x68;

    if (addr == 0x18)
        LSM303_addr_acc = 0x18;
    if (addr == 0x1d)
        LSM303_addr_mag = 0x1d;
    if (addr == 0x1e)
        LSM303_addr_mag = 0x1e;
}

uint8_t getDevAddr(SENSORS sensor)
{
    switch (sensor)
    {
    case BH1750:
        return BH1750_addr;
    case HMC5883L:
        return HMC5883L_addr;
    case ADXL345:
        return ADXL345_addr;
    case MPU925X:
        return MPU925X_addr;
    case BMx280:
        return BMx280_addr;
    case SHT3x:
        return SHT3x_addr;
    case LSM303_ACCEL:
        return LSM303_addr_acc;
    case LSM303_MAG:
        return LSM303_addr_mag;
    default:
        return 0;
        break;
    }
}