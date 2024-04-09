#ifndef __MPU925x_H_
#define __MPU925x_H_

#include <stdint.h>

#define WHO_AM_I_MPU9250 0x75
#define MPU9250_WHOAMI_DEFAULT_VALUE 0x71
#define MPU9255_WHOAMI_DEFAULT_VALUE 0x73
#define MPU6500_WHOAMI_DEFAULT_VALUE 0x70
#define MPU9255_PWR_MGMT_1 0x6B
#define MPU_CONFIG 0x1A
#define SMPLRT_DIV 0x19
#define MPU_GYRO_CONFIG 0x1B
#define MPU_ACCEL_CONFIG 0x1c
#define MPU_ACCEL_CONFIG2 0x1d
#define MPU_ACCEL_XOUT_H 0x3B
#define MPU_EXT_SENS_DATA_00 0x49
#define MPU_USER_CTRL 0x6A
#define MPU_I2C_MST_EN 0x20
#define MPU_I2C_MST_CTRL 0x24

esp_err_t mpu925x_init_desc(i2c_dev_t *dev, uint8_t addr, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio);
esp_err_t mpu925x_free_desc(i2c_dev_t *dev);

esp_err_t mpu925x_init(i2c_dev_t *dev);
esp_err_t mpu925x_read_accel(i2c_dev_t *dev, float *x, float *y, float *z);
esp_err_t mpu925x_read_mag(i2c_dev_t *dev, int16_t *hx, int16_t *hy, int16_t *hz);
#endif