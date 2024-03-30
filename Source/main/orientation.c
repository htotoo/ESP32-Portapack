
#include "orientation.h"
#include "nvs_flash.h"
#include <string.h>
#include "sensordb.h"

float declinationAngle = 0; // todo setup to web interface http://www.magnetic-declination.com/

hmc5883l_dev_t dev_hmc5883l;
i2c_dev_t dev_adxl345;

OrientationSensors orientation_inited = Orientation_none;
AcceloSensors accelo_inited = Accelo_none;

float accelo_x = 0;
float accelo_y = 0;
float accelo_z = 0;

int16_t orientationXMin = INT16_MAX;
int16_t orientationYMin = INT16_MAX;
int16_t orientationZMin = INT16_MAX;
int16_t orientationXMax = INT16_MIN;
int16_t orientationYMax = INT16_MIN;
int16_t orientationZMax = INT16_MIN;

void init_gyro()
{
    accelo_inited = Accelo_none;

    memset(&dev_adxl345, 0, sizeof(dev_adxl345));
    adxl345_init_desc(&dev_adxl345, getDevAddr(ADXL345), 0, CONFIG_IC2SDAPIN, CONFIG_IC2SCLPIN);
    if (adxl345_init(&dev_adxl345) == ESP_OK)
    {
        accelo_inited |= Accelo_ADXL345;
        ESP_LOGI("Gyro", "adxl345 OK");
    }
    else
    {
        adxl345_free_desc(&dev_adxl345);
    }
}

void init_orientation()
{
    // HCM5883l
    memset(&dev_hmc5883l, 0, sizeof(hmc5883l_dev_t));
    hmc5883l_init_desc(&dev_hmc5883l, 0, CONFIG_IC2SDAPIN, CONFIG_IC2SCLPIN, getDevAddr(HMC5883L));

    if (hmc5883l_init(&dev_hmc5883l) == ESP_OK)
    {
        hmc5883l_set_opmode(&dev_hmc5883l, HMC5883L_MODE_CONTINUOUS);
        hmc5883l_set_samples_averaged(&dev_hmc5883l, HMC5883L_SAMPLES_8);
        hmc5883l_set_data_rate(&dev_hmc5883l, HMC5883L_DATA_RATE_07_50);
        hmc5883l_set_gain(&dev_hmc5883l, HMC5883L_GAIN_1090);
        ESP_LOGI("Orientation", "hmc5883lTestConnection OK");
        orientation_inited = Orientation_hmc5883l;
    }
    else
    {
        hmc5883l_free_desc(&dev_hmc5883l);
    }

    // end of list
    if (orientation_inited == Orientation_none)
    {
        ESP_LOGI("Orientation", "No compatible sensor found");
        return;
    }

    init_gyro();
    // load calibration data
    load_config_orientation();
}

float fix_heading(float heading)
{
    if (heading < 0)
        heading += 2 * M_PI;

    if (heading > 2 * M_PI)
        heading -= 2 * M_PI;
    return (heading + declinationAngle);
}

void update_gyro_data()
{
    if (accelo_inited == Accelo_none)
        return;
    if ((accelo_inited && Accelo_ADXL345) == Accelo_ADXL345)
    {
        adxl345_read_x(&dev_adxl345, &accelo_x);
        adxl345_read_y(&dev_adxl345, &accelo_y);
        adxl345_read_z(&dev_adxl345, &accelo_z);
        // ESP_LOGI("accel", "accel data: %f %f %f", gyro_x, gyro_y, gyro_z);
    }
}

float get_heading()
{
    if (orientation_inited == Orientation_none)
        return 0;

    update_gyro_data(); // update only if orientation needs it.
    if (orientation_inited == Orientation_hmc5883l)
    {
        hmc5883l_data_t data;
        if (hmc5883l_get_data(&dev_hmc5883l, &data) == ESP_OK)
        {
            if (orientation_inited == Orientation_none)
                return atan2(data.y, data.x);
            // if got gyro data
            float accXnorm = accelo_x / sqrt(accelo_x * accelo_x + accelo_y * accelo_y + accelo_z * accelo_z);
            float accYnorm = accelo_y / sqrt(accelo_x * accelo_x + accelo_y * accelo_y + accelo_z * accelo_z);
            float pitch = asin(-accXnorm);
            float roll = asin(accYnorm / cos(pitch));

            // todo use calibration
            float magXcomp = data.x; // (magX - calibration[0]) / (calibration[1] - calibration[0]) * 2 - 1;
            float magYcomp = data.y; //(magY - calibration[2]) / (calibration[3] - calibration[2]) * 2 - 1;
            float magZcomp = data.z; //(magZ - calibration[4]) / (calibration[5] - calibration[4]) * 2 - 1;

            float magXheading = magXcomp * cos(pitch) + magZcomp * sin(pitch);
            float magYheading = magXcomp * sin(roll) * sin(pitch) + magYcomp * cos(roll) - magZcomp * sin(roll) * cos(pitch);

            return atan2(magYheading, magXheading);
        }
    }
    return 0;
}

float get_heading_degrees()
{
    if (orientation_inited == Orientation_none)
        return 400;
    float heading = get_heading() * 180 / M_PI + declinationAngle;
    if (heading < 0)
        heading += 360;
    return heading;
}

void calibrate_orientation(uint8_t sec)
{
    // todo, run a calibration for N sec to get min-max. should be around 10

    save_config_orientation();
}

void set_declination(float declination)
{
    declinationAngle = declination;
    save_config_orientation();
}
float get_declination() { return declinationAngle; }