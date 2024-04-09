// Some data from  https://github.com/arkhipenko/MPU9250/blob/master/MPU9250_t3.cpp
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include "esp_idf_lib_helpers.h"
#include "i2cdev.h"
#include "mpu925x.h"

#define I2C_FREQ_HZ 400000

static const char *TAG = "mpu925x";

#define CHECK_ARG(VAL)                  \
    do                                  \
    {                                   \
        if (!(VAL))                     \
            return ESP_ERR_INVALID_ARG; \
    } while (0)

esp_err_t mpu925x_init_desc(i2c_dev_t *dev, uint8_t addr, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio)
{
    CHECK_ARG(dev);

    if (addr != 0x68)
    {
        ESP_LOGE(TAG, "Invalid I2C address");
        return ESP_ERR_INVALID_ARG;
    }

    dev->port = port;
    dev->addr = addr;
    dev->cfg.sda_io_num = sda_gpio;
    dev->cfg.scl_io_num = scl_gpio;
#if HELPER_TARGET_IS_ESP32
    dev->cfg.master.clk_speed = I2C_FREQ_HZ;
#endif
    return i2c_dev_create_mutex(dev);
}

esp_err_t mpu925x_free_desc(i2c_dev_t *dev)
{
    CHECK_ARG(dev);
    return i2c_dev_delete_mutex(dev);
}

esp_err_t mpu925x_init(i2c_dev_t *dev)
{
    CHECK_ARG(dev);
    uint8_t devId = 0;
    esp_err_t err = i2c_dev_read_reg(dev, WHO_AM_I_MPU9250, &devId, sizeof(devId));
    if (err != ESP_OK)
    {
        return err;
    }

    bool b = (devId == MPU9250_WHOAMI_DEFAULT_VALUE);
    b |= (devId == MPU9255_WHOAMI_DEFAULT_VALUE);
    b |= (devId == MPU6500_WHOAMI_DEFAULT_VALUE);

    if (!b)
        return ESP_ERR_INVALID_ARG;
    // reset device
    uint8_t data = 0x80;
    i2c_dev_write_reg(dev, MPU9255_PWR_MGMT_1, &data, sizeof(data)); // 2g range
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // wake up device
    data = 0;
    i2c_dev_write_reg(dev, MPU9255_PWR_MGMT_1, &data, sizeof(data)); // 2g range
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // get stable time source
    data = 0x1;
    i2c_dev_write_reg(dev, MPU9255_PWR_MGMT_1, &data, sizeof(data)); // 2g range
    vTaskDelay(100 / portTICK_PERIOD_MS);
    // i2c host ena
    data = MPU_I2C_MST_EN;
    i2c_dev_write_reg(dev, MPU_USER_CTRL, &data, sizeof(data));
    // i2c speed
    data = 0x0D;
    i2c_dev_write_reg(dev, MPU_I2C_MST_CTRL, &data, sizeof(data));

    data = 3; // 41hz
    i2c_dev_write_reg(dev, MPU_CONFIG, &data, sizeof(data));
    data = 4; // SMPL_200HZ
    i2c_dev_write_reg(dev, SMPLRT_DIV, &data, sizeof(data));

    i2c_dev_read_reg(dev, MPU_GYRO_CONFIG, &data, sizeof(data)); // get current GYRO_CONFIG register value
    data = data & ~0xE0;                                         // Clear self-test bits [7:5]
    data = data & ~0x03;                                         // Clear Fchoice bits [1:0]
    data = data & ~0x18;                                         // Clear GYRO_FS_SEL bits [4:3]
    data = data | ((uint8_t)(3) << 3);                           // Set full scale range for the gyro
    data = data | ((uint8_t)(~(0x03)) & 0x03);                   // Set Fchoice for the gyro
    i2c_dev_write_reg(dev, MPU_GYRO_CONFIG, &data, sizeof(data));

    i2c_dev_read_reg(dev, MPU_ACCEL_CONFIG, &data, sizeof(data));  // get current ACCEL_CONFIG register value
    data = data & ~0xE0;                                           // Clear self-test bits [7:5]
    data = data & ~0x18;                                           // Clear ACCEL_FS_SEL bits [4:3]
    data = data | ((uint8_t)3 << 3);                               // Set full scale range for the accelerometer
    i2c_dev_write_reg(dev, MPU_ACCEL_CONFIG, &data, sizeof(data)); // Write new ACCEL_CONFIG register value

    i2c_dev_read_reg(dev, MPU_ACCEL_CONFIG2, &data, sizeof(data)); // get current ACCEL_CONFIG2 register value
    data = data & ~0x0F;                                           // Clear accel_fchoice_b (bit 3) and A_DLPFG (bits [2:0])
    data = data | (~(1 << 3) & 0x08);                              // Set accel_fchoice_b to 1
    data = data | ((uint8_t)3 & 0x07);                             // Set accelerometer rate to 1 kHz and bandwidth to 41 Hz
    i2c_dev_write_reg(dev, MPU_ACCEL_CONFIG, &data, sizeof(data));
    vTaskDelay(100 / portTICK_PERIOD_MS);
    return ESP_OK;
}

esp_err_t mpu925x_read_accel(i2c_dev_t *dev, float *x, float *y, float *z)
{
    float acc_resolution = 16.0 / 32768.0;
    uint8_t raw_data[14] = {0};
    int16_t destination[7];                                              // x/y/z accel register data stored here
    i2c_dev_read_reg(dev, MPU_ACCEL_XOUT_H, &raw_data[0], 14);           // Read the 14 raw data registers into data array
    destination[0] = ((int16_t)raw_data[0] << 8) | (int16_t)raw_data[1]; // Turn the MSB and LSB into a signed 16-bit value
    destination[1] = ((int16_t)raw_data[2] << 8) | (int16_t)raw_data[3];
    destination[2] = ((int16_t)raw_data[4] << 8) | (int16_t)raw_data[5];
    destination[3] = ((int16_t)raw_data[6] << 8) | (int16_t)raw_data[7];
    destination[4] = ((int16_t)raw_data[8] << 8) | (int16_t)raw_data[9];
    destination[5] = ((int16_t)raw_data[10] << 8) | (int16_t)raw_data[11];
    destination[6] = ((int16_t)raw_data[12] << 8) | (int16_t)raw_data[13];

    *x = (float)destination[0] * acc_resolution; // get actual g value, this depends on scale being set
    *y = (float)destination[1] * acc_resolution;
    *z = (float)destination[2] * acc_resolution;
    return ESP_OK;
}

esp_err_t mpu925x_read_mag(i2c_dev_t *dev, int16_t *hx, int16_t *hy, int16_t *hz)
{
    uint8_t buff[7];
    // read the magnetometer data off the external sensor buffer
    i2c_dev_read_reg(dev, MPU_EXT_SENS_DATA_00, &buff[0], sizeof(buff));
    if (buff[6] == 0x10)
    {                                              // check for overflow
        *hx = (((int16_t)buff[1]) << 8) | buff[0]; // combine into 16 bit values
        *hy = (((int16_t)buff[3]) << 8) | buff[2];
        *hz = (((int16_t)buff[5]) << 8) | buff[4];
        return ESP_OK;
    }
    else
    {
        *hx = 0;
        *hy = 0;
        *hz = 0;
        return ESP_ERR_INVALID_STATE;
    }
}