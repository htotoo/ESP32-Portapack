/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ruslan V. Uss <unclerus@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file i2cdev.c
 *
 * Thread-safe I2C master helpers built on the ESP-IDF handle-based driver.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "i2cdev.h"

static const char* TAG = "i2cdev";

typedef struct
{
    SemaphoreHandle_t lock;
    i2c_master_bus_handle_t bus;
    gpio_num_t sda_io_num;
    gpio_num_t scl_io_num;
    bool enable_internal_pullup;
} i2c_port_state_t;

static i2c_port_state_t states[I2C_NUM_MAX];
static bool initialized;

static esp_err_t lock_port(i2c_port_num_t port)
{
    if (port < I2C_NUM_0 || port >= I2C_NUM_MAX)
        return ESP_ERR_INVALID_ARG;
    if (!initialized || !states[port].lock)
        return ESP_ERR_INVALID_STATE;
    if (!xSemaphoreTake(states[port].lock, pdMS_TO_TICKS(CONFIG_I2CDEV_TIMEOUT))) {
        ESP_LOGE(TAG, "Could not take port mutex %d", port);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static esp_err_t unlock_port(i2c_port_num_t port)
{
    if (!xSemaphoreGive(states[port].lock)) {
        ESP_LOGE(TAG, "Could not give port mutex %d", port);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t i2c_setup_port(const i2c_dev_t* dev)
{
    if (dev->port < I2C_NUM_0 || dev->port >= I2C_NUM_MAX || dev->addr > 0x7f ||
        dev->scl_speed_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_port_state_t* state = &states[dev->port];
    if (state->bus &&
        (state->sda_io_num != dev->sda_io_num || state->scl_io_num != dev->scl_io_num ||
         state->enable_internal_pullup != dev->enable_internal_pullup)) {
        esp_err_t err = i2c_del_master_bus(state->bus);
        if (err != ESP_OK)
            return err;
        state->bus = NULL;
    }

    if (!state->bus) {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = dev->port,
            .sda_io_num = dev->sda_io_num,
            .scl_io_num = dev->scl_io_num,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags.enable_internal_pullup = dev->enable_internal_pullup,
        };
        esp_err_t err = i2c_new_master_bus(&bus_config, &state->bus);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not create I2C bus on port %d: %s", dev->port, esp_err_to_name(err));
            return err;
        }
        state->sda_io_num = dev->sda_io_num;
        state->scl_io_num = dev->scl_io_num;
        state->enable_internal_pullup = dev->enable_internal_pullup;
    }

    return ESP_OK;
}

static esp_err_t add_device(const i2c_dev_t* dev, i2c_master_dev_handle_t* handle)
{
    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev->addr,
        .scl_speed_hz = dev->scl_speed_hz,
    };
    return i2c_master_bus_add_device(states[dev->port].bus, &device_config, handle);
}

static esp_err_t remove_device(i2c_master_dev_handle_t handle, esp_err_t operation_result)
{
    esp_err_t err = i2c_master_bus_rm_device(handle);
    return operation_result == ESP_OK ? err : operation_result;
}

esp_err_t i2cdev_init()
{
    if (initialized)
        return ESP_OK;

    memset(states, 0, sizeof(states));
    for (int i = 0; i < I2C_NUM_MAX; i++) {
        states[i].lock = xSemaphoreCreateMutex();
        if (!states[i].lock) {
            ESP_LOGE(TAG, "Could not create port mutex %d", i);
            for (int j = 0; j < i; j++) {
                vSemaphoreDelete(states[j].lock);
                states[j].lock = NULL;
            }
            return ESP_FAIL;
        }
    }
    initialized = true;
    return ESP_OK;
}

esp_err_t i2cdev_done()
{
    if (!initialized)
        return ESP_OK;

    for (int i = 0; i < I2C_NUM_MAX; i++) {
        esp_err_t err = lock_port((i2c_port_num_t)i);
        if (err != ESP_OK)
            return err;
        if (states[i].bus) {
            err = i2c_del_master_bus(states[i].bus);
            if (err != ESP_OK) {
                unlock_port((i2c_port_num_t)i);
                return err;
            }
            states[i].bus = NULL;
        }
        err = unlock_port((i2c_port_num_t)i);
        if (err != ESP_OK)
            return err;
        vSemaphoreDelete(states[i].lock);
        states[i].lock = NULL;
    }
    initialized = false;
    return ESP_OK;
}

esp_err_t i2c_dev_create_mutex(i2c_dev_t* dev)
{
#if !CONFIG_I2CDEV_NOLOCK
    if (!dev)
        return ESP_ERR_INVALID_ARG;
    dev->mutex = xSemaphoreCreateMutex();
    if (!dev->mutex) {
        ESP_LOGE(TAG, "[0x%02x at %d] Could not create device mutex", dev->addr, dev->port);
        return ESP_FAIL;
    }
#else
    (void)dev;
#endif
    return ESP_OK;
}

esp_err_t i2c_dev_delete_mutex(i2c_dev_t* dev)
{
#if !CONFIG_I2CDEV_NOLOCK
    if (!dev || !dev->mutex)
        return ESP_ERR_INVALID_ARG;
    vSemaphoreDelete(dev->mutex);
    dev->mutex = NULL;
#else
    (void)dev;
#endif
    return ESP_OK;
}

esp_err_t i2c_dev_take_mutex(i2c_dev_t* dev)
{
#if !CONFIG_I2CDEV_NOLOCK
    if (!dev || !dev->mutex)
        return ESP_ERR_INVALID_ARG;
    if (!xSemaphoreTake(dev->mutex, pdMS_TO_TICKS(CONFIG_I2CDEV_TIMEOUT))) {
        ESP_LOGE(TAG, "[0x%02x at %d] Could not take device mutex", dev->addr, dev->port);
        return ESP_ERR_TIMEOUT;
    }
#else
    (void)dev;
#endif
    return ESP_OK;
}

esp_err_t i2c_dev_give_mutex(i2c_dev_t* dev)
{
#if !CONFIG_I2CDEV_NOLOCK
    if (!dev || !dev->mutex)
        return ESP_ERR_INVALID_ARG;
    if (!xSemaphoreGive(dev->mutex)) {
        ESP_LOGE(TAG, "[0x%02x at %d] Could not give device mutex", dev->addr, dev->port);
        return ESP_FAIL;
    }
#else
    (void)dev;
#endif
    return ESP_OK;
}

esp_err_t i2c_dev_probe(const i2c_dev_t* dev, i2c_dev_type_t operation_type)
{
    if (!dev || (operation_type != I2C_DEV_WRITE && operation_type != I2C_DEV_READ))
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = lock_port(dev->port);
    if (err != ESP_OK)
        return err;
    err = i2c_setup_port(dev);
    if (err == ESP_OK)
        err = i2c_master_probe(states[dev->port].bus, dev->addr, CONFIG_I2CDEV_TIMEOUT);
    esp_err_t unlock_err = unlock_port(dev->port);
    return err == ESP_OK ? unlock_err : err;
}

esp_err_t i2c_dev_read(const i2c_dev_t* dev, const void* out_data, size_t out_size, void* in_data, size_t in_size)
{
    if (!dev || !in_data || !in_size)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = lock_port(dev->port);
    if (err != ESP_OK)
        return err;
    err = i2c_setup_port(dev);
    i2c_master_dev_handle_t handle = NULL;
    if (err == ESP_OK)
        err = add_device(dev, &handle);
    if (err == ESP_OK) {
        if (out_data && out_size) {
            err = i2c_master_transmit_receive(handle, out_data, out_size, in_data, in_size,
                                              CONFIG_I2CDEV_TIMEOUT);
        } else {
            err = i2c_master_receive(handle, in_data, in_size, CONFIG_I2CDEV_TIMEOUT);
        }
        if (err != ESP_OK)
            ESP_LOGE(TAG, "Could not read from device [0x%02x at %d]: %d (%s)",
                     dev->addr, dev->port, err, esp_err_to_name(err));
        err = remove_device(handle, err);
    }
    esp_err_t unlock_err = unlock_port(dev->port);
    return err == ESP_OK ? unlock_err : err;
}

esp_err_t i2c_dev_write(const i2c_dev_t* dev, const void* out_reg, size_t out_reg_size,
                        const void* out_data, size_t out_size)
{
    if (!dev || !out_data || !out_size || (out_reg_size && !out_reg) ||
        out_reg_size > SIZE_MAX - out_size) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t transfer_size = out_reg_size + out_size;
    uint8_t* transfer_data = malloc(transfer_size);
    if (!transfer_data)
        return ESP_ERR_NO_MEM;
    if (out_reg_size)
        memcpy(transfer_data, out_reg, out_reg_size);
    memcpy(transfer_data + out_reg_size, out_data, out_size);

    esp_err_t err = lock_port(dev->port);
    if (err != ESP_OK) {
        free(transfer_data);
        return err;
    }
    err = i2c_setup_port(dev);
    i2c_master_dev_handle_t handle = NULL;
    if (err == ESP_OK)
        err = add_device(dev, &handle);
    if (err == ESP_OK) {
        err = i2c_master_transmit(handle, transfer_data, transfer_size, CONFIG_I2CDEV_TIMEOUT);
        if (err != ESP_OK)
            ESP_LOGE(TAG, "Could not write to device [0x%02x at %d]: %d (%s)",
                     dev->addr, dev->port, err, esp_err_to_name(err));
        err = remove_device(handle, err);
    }
    esp_err_t unlock_err = unlock_port(dev->port);
    free(transfer_data);
    return err == ESP_OK ? unlock_err : err;
}

esp_err_t i2c_dev_read_reg(const i2c_dev_t* dev, uint8_t reg, void* in_data, size_t in_size)
{
    return i2c_dev_read(dev, &reg, 1, in_data, in_size);
}

esp_err_t i2c_dev_write_reg(const i2c_dev_t* dev, uint8_t reg, const void* out_data, size_t out_size)
{
    return i2c_dev_write(dev, &reg, 1, out_data, out_size);
}
