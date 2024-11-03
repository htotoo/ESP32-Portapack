#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include "esp_idf_lib_helpers.h"
#include "i2cdev.h"
#include "adxl345.h"

#define I2C_FREQ_HZ 400000

#define CHECK_ARG(VAL)                  \
    do                                  \
    {                                   \
        if (!(VAL))                     \
            return ESP_ERR_INVALID_ARG; \
    } while (0)

esp_err_t adxl345_init_desc(i2c_dev_t *dev, uint8_t addr, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio)
{
    CHECK_ARG(dev);

    if (addr != 0x53 && addr != 0x1D)
    {
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

esp_err_t adxl345_free_desc(i2c_dev_t *dev)
{
    CHECK_ARG(dev);
    return i2c_dev_delete_mutex(dev);
}

esp_err_t adxl345_init(i2c_dev_t *dev)
{
    CHECK_ARG(dev);
    uint8_t devId = 0;
    esp_err_t err = i2c_dev_read_reg(dev, ADXL345_DEVID, &devId, sizeof(devId));
    if (err != ESP_OK)
    {
        return err;
    }
    uint8_t data = 0b00000000;
    i2c_dev_write_reg(dev, ADXL345_INT_ENABLE, &data, sizeof(data)); // Disable int
    data = 0b00001000;
    i2c_dev_write_reg(dev, ADXL345_DATAFORMAT, &data, sizeof(data)); // 2g range
    data = 0;
    i2c_dev_write_reg(dev, ADXL345_FIFO_CTL, &data, sizeof(data)); // skip fifo
    data = 0b00001000;
    i2c_dev_write_reg(dev, ADXL345_POWER_CTL, &data, sizeof(data)); // Enable measure mode

    return ESP_OK;
}

esp_err_t adxl345_read_x(i2c_dev_t *dev, float *x)
{
    int16_t tmp;
    esp_err_t err = i2c_dev_read_reg(dev, ADXL345_DATAX, &tmp, 2);
    // tmp = __bswap16(tmp);
    *x = tmp * ADXL345_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    return err;
}

esp_err_t adxl345_read_y(i2c_dev_t *dev, float *y)
{
    int16_t tmp;
    esp_err_t err = i2c_dev_read_reg(dev, ADXL345_DATAY, &tmp, 2);
    // tmp = __bswap16(tmp);
    *y = tmp * ADXL345_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    return err;
}
esp_err_t adxl345_read_z(i2c_dev_t *dev, float *z)
{
    int16_t tmp;
    esp_err_t err = i2c_dev_read_reg(dev, ADXL345_DATAZ, &tmp, 2);
    // tmp = __bswap16(tmp);
    *z = tmp * ADXL345_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    return err;
}
