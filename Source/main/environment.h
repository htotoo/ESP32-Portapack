#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <math.h>
#include "esp_log.h"
// include supported modules
#include "drivers/i2cdev.h"

#include "drivers/bmp280.h"

typedef enum EnvironmentSensors
{
    Environment_none = 0,
    Environment_bmp280 = 1,
    Environment_bme280 = 2
} EnvironmentSensors;

void init_environment();

void get_environment_meas(float *temperature, float *pressure, float *humidity);

#endif