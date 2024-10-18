#ifndef ORIENTATION_H
#define ORIENTATION_H

#include <math.h>

#include "configuration.h"
#include "esp_log.h"




// include supported modules
#include "drivers/i2cdev.h"

#include "drivers/hmc5883l.h" //only 2d and maybe the ADXL345 for 3d
#include "drivers/adxl345.h"  //gyro for hmc5883l
#include "drivers/mpu925x.h"  //maybe faulty!
#include "drivers/lsm303.h"

#define M_PI 3.14159265358979323846

typedef enum OrientationSensors
{
    Orientation_none = 0,
    Orientation_hmc5883l = 1,
    Orientation_mpu925x = 2,
    Orientation_lsm303 = 4,
} OrientationSensors;

typedef enum AcceloSensors
{
    Accelo_none = 0,
    Accelo_ADXL345 = 1,
    Accelo_MPU925x = 2,
    Accelo_LSM303 = 4
} AcceloSensors;

#if __cplusplus
extern "C"
{
#endif

void init_orientation();

float get_heading();
float get_heading_degrees();

void calibrate_orientation(uint8_t sec);

void set_declination(float declination);
float get_declination();

float get_tilt();

#if __cplusplus
}
#endif

#endif