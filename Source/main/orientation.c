
#include "orientation.h"
#include "nvs_flash.h"
#include <string.h>
#include "sensordb.h"

float declinationAngle = 0; // todo setup to web interface https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml

hmc5883l_dev_t dev_hmc5883l;
i2c_dev_t dev_adxl345;
i2c_dev_t dev_mpu925x;
lsm303_t dev_lsm303;

OrientationSensors orientation_inited = Orientation_none;
AcceloSensors accelo_inited = Accelo_none;

float accelo_x = 0;
float accelo_y = 0;
float accelo_z = 0;

float m_tilt = 400.0;

int16_t orientationXMin = INT16_MAX;
int16_t orientationYMin = INT16_MAX;
int16_t orientationZMin = INT16_MAX;
int16_t orientationXMax = INT16_MIN;
int16_t orientationYMax = INT16_MIN;
int16_t orientationZMax = INT16_MIN;

void init_accelo()
{
    memset(&dev_adxl345, 0, sizeof(dev_adxl345));
    adxl345_init_desc(&dev_adxl345, getDevAddr(ADXL345), 0, CONFIG_IC2SDAPIN, CONFIG_IC2SCLPIN);
    if (adxl345_init(&dev_adxl345) == ESP_OK)
    {
        accelo_inited |= Accelo_ADXL345;
        ESP_LOGI("Accel", "adxl345 OK");
    }
    else
    {
        adxl345_free_desc(&dev_adxl345);
    }
}

void init_orientation()
{
    // load calibration data
    load_config_orientation();
    accelo_inited = Accelo_none;
    orientation_inited = Orientation_none;
    /* HCM TEMP DISABLED
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
        orientation_inited |= Orientation_hmc5883l;
    }
    else
    {
        hmc5883l_free_desc(&dev_hmc5883l);
    }
    */
    // MPU9250
    memset(&dev_mpu925x, 0, sizeof(dev_mpu925x));
    mpu925x_init_desc(&dev_mpu925x, getDevAddr(MPU925X), 0, CONFIG_IC2SDAPIN, CONFIG_IC2SCLPIN);
    if (mpu925x_init(&dev_mpu925x) == ESP_OK)
    {
        accelo_inited |= Accelo_MPU925x; // todo check if really availeable
        orientation_inited |= Orientation_mpu925x;
        ESP_LOGI("Orient", "mpu925x OK");
    }
    else
    {
        mpu925x_free_desc(&dev_mpu925x);
    }

    memset(&dev_lsm303, 0, sizeof(dev_lsm303));
    lsm303_init_desc(&dev_lsm303, getDevAddr(LSM303_ACCEL), getDevAddr(LSM303_MAG), 0, CONFIG_IC2SDAPIN, CONFIG_IC2SCLPIN);
    if (lsm303_init(&dev_lsm303) == ESP_OK)
    {
        accelo_inited |= Accelo_LSM303;
        orientation_inited |= Orientation_lsm303;
        lsm303_acc_set_config(&dev_lsm303, LSM303_ACC_MODE_NORMAL, LSM303_ODR_100_HZ, LSM303_ACC_SCALE_2G);
        lsm303_mag_set_config(&dev_lsm303, LSM303_MAG_MODE_CONT, LSM303_MAG_RATE_15, LSM303_MAG_GAIN_1_3);
        ESP_LOGI("Orient", "LSM303 OK");
    }
    else
    {
        lsm303_free_desc(&dev_lsm303);
        ESP_LOGI("Orient", "LSM303 failed");
    }

    // end of list
    if (orientation_inited == Orientation_none)
    {
        ESP_LOGI("Orientation", "No compatible sensor found");
        return;
    }

    init_accelo();
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
    if ((accelo_inited & Accelo_ADXL345) == Accelo_ADXL345)
    {
        adxl345_read_x(&dev_adxl345, &accelo_x);
        adxl345_read_y(&dev_adxl345, &accelo_y);
        adxl345_read_z(&dev_adxl345, &accelo_z);
        ESP_LOGI("accel", "accel data: %f %f %f", accelo_x, accelo_y, accelo_z);
    }

    if ((accelo_inited & Accelo_MPU925x) == Accelo_MPU925x)
    {
        mpu925x_read_accel(&dev_mpu925x, &accelo_x, &accelo_y, &accelo_z);
        ESP_LOGI("accel2", "accel data: %f %f %f", accelo_x, accelo_y, accelo_z);
    }

    if ((accelo_inited & Accelo_LSM303) == Accelo_LSM303)
    {
        lsm303_acc_raw_data_t acc_raw;
        lsm303_acc_data_t acc;

        if (lsm303_acc_get_raw_data(&dev_lsm303, &acc_raw) == ESP_OK)
        {
            lsm303_acc_raw_to_g(&dev_lsm303, &acc_raw, &acc);
            accelo_x = acc.x;
            accelo_y = acc.y;
            accelo_z = acc.z;
            ESP_LOGI("accel3", "accel data: %f %f %f", accelo_x, accelo_y, accelo_z);
        }
    }
}

float get_heading()
{
    if (orientation_inited == Orientation_none)
        return 0;
    float ret = 0.0;
    update_gyro_data(); // update only if orientation needs it.
    if ((orientation_inited & Orientation_hmc5883l) == Orientation_hmc5883l)
    {
        hmc5883l_data_t data;
        if (hmc5883l_get_data(&dev_hmc5883l, &data) == ESP_OK)
        {
            if (orientation_inited == Orientation_none)
                ret = atan2(data.y, data.x);
            else
            {
                // if got gyro data
                float accXnorm = accelo_x / sqrt(accelo_x * accelo_x + accelo_y * accelo_y + accelo_z * accelo_z);
                float accYnorm = accelo_y / sqrt(accelo_x * accelo_x + accelo_y * accelo_y + accelo_z * accelo_z);
                float pitch = asin(-accXnorm);
                float roll = asin(accYnorm / cos(pitch));
                m_tilt = roll * 2 * M_PI;
                // todo use calibration
                float magXcomp = data.x; // (magX - calibration[0]) / (calibration[1] - calibration[0]) * 2 - 1;
                float magYcomp = data.y; //(magY - calibration[2]) / (calibration[3] - calibration[2]) * 2 - 1;
                float magZcomp = data.z; //(magZ - calibration[4]) / (calibration[5] - calibration[4]) * 2 - 1;

                float magXheading = magXcomp * cos(pitch) + magZcomp * sin(pitch);
                float magYheading = magXcomp * sin(roll) * sin(pitch) + magYcomp * cos(roll) - magZcomp * sin(roll) * cos(pitch);

                ret = atan2(magYheading, magXheading);
            }
        }
    }
    if ((orientation_inited & Orientation_mpu925x) == Orientation_mpu925x)
    {
        int16_t magX = 0;
        int16_t magY = 0;
        int16_t magZ = 0;
        if (mpu925x_read_mag(&dev_mpu925x, &magX, &magY, &magZ) == ESP_OK)
        {
            ESP_LOGI("Orientation_mpu925x", "mxyz: %d %d %d", magX, magY, magZ);
            if (orientation_inited == Orientation_none || (accelo_x == 0 && accelo_y == 0))
                ret = atan2(magY, magX);
            else
            {

                // if got gyro data
                float accXnorm = accelo_x / sqrt(accelo_x * accelo_x + accelo_y * accelo_y + accelo_z * accelo_z);
                float accYnorm = accelo_y / sqrt(accelo_x * accelo_x + accelo_y * accelo_y + accelo_z * accelo_z);
                float pitch = asin(-accXnorm);
                float roll = asin(accYnorm / cos(pitch));
                m_tilt = roll * 2 * M_PI;
                // todo use calibration
                float magXcomp = magX; // (magX - calibration[0]) / (calibration[1] - calibration[0]) * 2 - 1;
                float magYcomp = magY; //(magY - calibration[2]) / (calibration[3] - calibration[2]) * 2 - 1;
                float magZcomp = magZ; //(magZ - calibration[4]) / (calibration[5] - calibration[4]) * 2 - 1;

                float magXheading = magXcomp * cos(pitch) + magZcomp * sin(pitch);
                float magYheading = magXcomp * sin(roll) * sin(pitch) + magYcomp * cos(roll) - magZcomp * sin(roll) * cos(pitch);

                ret = atan2(magYheading, magXheading);
            }
        }
        else
        {
            ESP_LOGI("Orientation_mpu925x", "Orientation_mpu925x ERROR");
        }
    }

    if ((orientation_inited & Orientation_lsm303) == Orientation_lsm303)
    {
        lsm303_mag_raw_data_t mag_raw;
        lsm303_mag_data_t mag;

        int16_t magX = 0;
        int16_t magY = 0;
        int16_t magZ = 0;
        if (lsm303_mag_get_raw_data(&dev_lsm303, &mag_raw) == ESP_OK)
        {
            lsm303_mag_raw_to_uT(&dev_lsm303, &mag_raw, &mag);
            magX = mag.x;
            magY = mag.y;
            magZ = mag.z;

            ESP_LOGI("Orientation_lsm", "mxyz: %d %d %d", magX, magY, magZ);
            if (orientation_inited == Orientation_none || (accelo_x == 0 && accelo_y == 0))
                ret = atan2(magY, magX);
            else
            {
                // if got gyro data
                float accXnorm = accelo_x / sqrt(accelo_x * accelo_x + accelo_y * accelo_y + accelo_z * accelo_z);
                float accYnorm = accelo_y / sqrt(accelo_x * accelo_x + accelo_y * accelo_y + accelo_z * accelo_z);
                float pitch = asin(-accXnorm);
                float roll = asin(accYnorm / cos(pitch));
                m_tilt = roll * 2 * M_PI;
                // todo use calibration
                float magXcomp = magX; // (magX - calibration[0]) / (calibration[1] - calibration[0]) * 2 - 1;
                float magYcomp = magY; //(magY - calibration[2]) / (calibration[3] - calibration[2]) * 2 - 1;
                float magZcomp = magZ; //(magZ - calibration[4]) / (calibration[5] - calibration[4]) * 2 - 1;

                float magXheading = magXcomp * cos(pitch) + magZcomp * sin(pitch);
                float magYheading = magXcomp * sin(roll) * sin(pitch) + magYcomp * cos(roll) - magZcomp * sin(roll) * cos(pitch);

                ret = atan2(magYheading, magXheading);
            }
        }
        else
        {
            ESP_LOGI("Orientation_lsm", "Orientation_lsm ERROR");
        }
    }
    ESP_LOGI("Orientation", "%f", ret);
    return ret;
}

float get_heading_degrees()
{
    if (orientation_inited == Orientation_none)
        return 400;
    float heading = get_heading() * 180 / M_PI + declinationAngle;
    if (heading < 0)
        heading += 360;
    ESP_LOGI("heading", "%f", heading);
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

float get_tilt() { return m_tilt; }

bool is_orientation_sensor_present()
{
    return orientation_inited != Orientation_none;
}