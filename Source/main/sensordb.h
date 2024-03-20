#ifndef __SENSORDB_H__
#define __SENSORDB_H__

// This file is a DB for found I2C device addresses.
// SENSORS:
// - BH1750  - 0x23
// - HMC5883L  - 0x1E  - PARTLY
// - ADXL345 - 0x53  //https://github.com/craigpeacock/ESP32_Node/blob/master/main/adxl345.h
// - MPU925X ( 0x68 ) + 280  ( 0x76 )

#include <inttypes.h>

typedef enum
{
    BH1750 = 0,
    HMC5883L,
    ADXL345,
    MPU925X,
    BMx280,
    SHT3x
} SENSORS;

void foundI2CDev(uint8_t addr);
uint8_t getDevAddr(SENSORS sensor);
#endif