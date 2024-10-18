/*
   Copyright 2024 Unicom Engineering, all rights reserved

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   See the .c file for more notes.
 */
#pragma once

#if __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include "soc/gpio_num.h"
#include "esp_err.h"

    typedef enum I2CSlaveCallbackReason
    {
        I2C_CALLBACK_REPEAT_START, // The master sent data, and has done a repeat start to read data.
        I2C_CALLBACK_SEND_DATA,    // The master is asking for us to send data, use i2c_slave_send_data.
        I2C_CALLBACK_DONE,         // transaction is complete (stop condition seen)
    } I2CSlaveCallbackReason;

    struct i2c_slave_device_t;

    // the callback function you should define. The return value is currently
    // ignored.
    typedef bool (*i2c_slave_callback_fn)(struct i2c_slave_device_t *dev, I2CSlaveCallbackReason reason);

    typedef enum I2CState
    {
        I2C_STATE_IDLE, // slave is idle, mostly used internally.
        I2C_STATE_RECV, // slave has received data from the master
        I2C_STATE_SEND  // slave is sending data to the master
    } I2CState;

    // buffer contains all the data sent or received in the current transaction.
    // bufstart denotes the start of the valid data, and bufend denotes the end of
    // the valid data.
    //
    // When sending, data is sent from bufstart, which is incremented, until it
    // reaches bufend. The user adds data to the end of the buffer using the
    // i2c_slave_send_data
    //
    // When receiving, data is added to bufend which is incremented, and the user
    // can use the bufstart to remember how much has been processed.
    typedef struct i2c_slave_device_t
    {
        uint8_t buffer[128];
        uint8_t bufend;
        uint8_t bufstart;
        I2CState state;
    } i2c_slave_device_t;

    typedef struct i2c_slave_config_t
    {
        i2c_slave_callback_fn callback;
        uint8_t address;     // slave address (7 bits)
        gpio_num_t gpio_scl; // scl gpio pin to use
        gpio_num_t gpio_sda; // sda gpio pin to use
        uint8_t i2c_port;    // which port to use
    } i2c_slave_config_t;

    // create a slave on an i2c port
    esp_err_t i2c_slave_new(i2c_slave_config_t *config, i2c_slave_device_t **result);

    // delete a slave on an i2c port.
    esp_err_t i2c_slave_del(i2c_slave_device_t *dev);

    // send data to the master. The driver must be in the I2C_STATE_SEND state.
    // Data is copied to the internal buffer. Data that does not fit in the buffer
    // is ignored.
    esp_err_t i2c_slave_send_data(i2c_slave_device_t *dev, uint8_t *buf, uint8_t len);

#if __cplusplus
}
#endif