#ifndef ORIENTATION_H
#define ORIENTATION_H

#include <math.h>
#include "esp_log.h"
// include supported modules
#include "drivers/i2cdev.h"

#include "drivers/hmc5883l.h" //only 2d and maybe the ADXL345 for 3d
#include "drivers/adxl345.h"  //gyro for hmc5883l

// MPU925X --check, accelo and 3d!

#define M_PI 3.14159265358979323846

typedef enum OrientationSensors
{
    Orientation_none = 0,
    Orientation_hmc5883l = 1
} OrientationSensors;

typedef enum GyroSensors
{
    Gyro_none = 0,
    Gyro_ADXL345 = 1
} GyroSensors;

void init_orientation();

float get_heading();
float get_heading_degrees();

void reset_orientation_calibration();

void calibrate_orientation(uint8_t sec);

void set_declination(float declination);
float get_declination();

#endif