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

   Note to users: this was tested using ESP-IDF version 5.3.1, and the Esp32s2
   chip. It has some calls that are Esp32-S2 specific, because the HAL does not
   support certain features that are supported by the hardware. Therefore, you
   may have to adjust some calls to get this to work with your device. Either
   use HAL functions that support the calls, or rewrite using your device's
   registers. Contributions to get this ported to other devices are welcome!

   Much of the setup is copied from the existing slave driver. I'm not sure
   exactly what each thing does, but I tried to copy all of the needed calls
   from i2c_common.c

   Also, I did not spend a lot of time on configuration, I mostly just
   supported our use case. Pull requests to add configuration options are also
   welcome!
 */
#include <sys/param.h>
#include "i2c_slave_driver.h"
#include "esp_attr.h"
#include "esp_intr_types.h"
#include "hal/i2c_hal.h"
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "esp_private/gpio.h"
#include "esp_private/periph_ctrl.h"
#include "esp_heap_caps.h"
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "i2c.unicom_slave";

typedef struct
{
    i2c_slave_device_t user_dev;
    int portNum;
    i2c_slave_callback_fn callback;
    intr_handle_t intr;
    gpio_num_t scl;
    gpio_num_t sda;
    i2c_hal_context_t hal;
    bool allocated; // is this device used
} i2c_slave_dev_private_t;

static i2c_slave_dev_private_t i2cdev[2] = {0};

static IRAM_ATTR void s_i2c_reset_buffer(i2c_slave_dev_private_t *i2c_slave)
{
    i2c_slave->user_dev.bufstart = 0;
    i2c_slave->user_dev.bufend = 0;
}

static esp_err_t s_hp_i2c_pins_config(i2c_slave_dev_private_t *handle)
{
    int port_id = handle->portNum;

    // SDA pin configurations
    gpio_config_t sda_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_down_en = false,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pin_bit_mask = 1ULL << handle->sda,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level(handle->sda, 1), TAG, "i2c sda pin set level failed");
    ESP_RETURN_ON_ERROR(gpio_config(&sda_conf), TAG, "config GPIO failed");
    gpio_func_sel(handle->sda, PIN_FUNC_GPIO);
    esp_rom_gpio_connect_out_signal(handle->sda, i2c_periph_signal[port_id].sda_out_sig, 0, 0);
    esp_rom_gpio_connect_in_signal(handle->sda, i2c_periph_signal[port_id].sda_in_sig, 0);

    // SCL pin configurations
    gpio_config_t scl_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_down_en = false,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pin_bit_mask = 1ULL << handle->scl,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level(handle->scl, 1), TAG, "i2c scl pin set level failed");
    ESP_RETURN_ON_ERROR(gpio_config(&scl_conf), TAG, "config GPIO failed");
    gpio_func_sel(handle->scl, PIN_FUNC_GPIO);
    esp_rom_gpio_connect_out_signal(handle->scl, i2c_periph_signal[port_id].scl_out_sig, 0, 0);
    esp_rom_gpio_connect_in_signal(handle->scl, i2c_periph_signal[port_id].scl_in_sig, 0);

    return ESP_OK;
}

static IRAM_ATTR void s_i2c_handle_rx_fifo_wm(i2c_slave_dev_private_t *i2c_slave)
{
    // read all data out of the rx fifo queue.
    i2c_hal_context_t *hal = &i2c_slave->hal;
    i2c_slave_device_t *s = &i2c_slave->user_dev;
    // can't be in send mode here.
    uint32_t rx_fifo_cnt;
    i2c_ll_get_rxfifo_cnt(hal->dev, &rx_fifo_cnt);
    uint32_t fifo_cnt_rd = MIN(sizeof(s->buffer) - s->bufend, rx_fifo_cnt);
    if (fifo_cnt_rd > 0)
    {
        ESP_ERROR_CHECK(s->state == I2C_STATE_SEND ? ESP_ERR_INVALID_STATE : ESP_OK);
        i2c_ll_read_rxfifo(hal->dev, s->buffer + s->bufend, fifo_cnt_rd);
        s->bufend += fifo_cnt_rd;
        s->state = I2C_STATE_RECV;
    }

    if (rx_fifo_cnt > fifo_cnt_rd)
    {
        // throw away additional bytes, we don't have a place to put them
        i2c_ll_rxfifo_rst(hal->dev);
    }
}

static IRAM_ATTR void s_i2c_handle_complete(i2c_slave_dev_private_t *i2c_slave)
{
    i2c_hal_context_t *hal = &i2c_slave->hal;
    i2c_slave_device_t *s = &i2c_slave->user_dev;
    // check if we have received any data. If so, this is an rx complete
    if (s->state == I2C_STATE_SEND)
    {
        // sending, check to see if there were bytes that didn't get sent.
        uint32_t tx_fifo_len;
        i2c_ll_get_txfifo_len(hal->dev, &tx_fifo_len);
        tx_fifo_len = SOC_I2C_FIFO_LEN - tx_fifo_len;
        if (tx_fifo_len > 0)
        {
            s->bufstart -= tx_fifo_len;
            // clear the fifo
            i2c_ll_txfifo_rst(hal->dev);
        }
    }
    else
    {
        // read all remaining fifo data
        s_i2c_handle_rx_fifo_wm(i2c_slave);
    }
    if (s->state != I2C_STATE_IDLE)
        i2c_slave->callback(s, I2C_CALLBACK_DONE);

    // reset the state of the slave device, it's about to go idle.
    i2c_ll_slave_disable_tx_it(hal->dev);
    s_i2c_reset_buffer(i2c_slave);
    s->state = I2C_STATE_IDLE;
}

static IRAM_ATTR void s_i2c_handle_tx_fifo_wm(i2c_slave_dev_private_t *i2c_slave)
{
    i2c_hal_context_t *hal = &i2c_slave->hal;
    i2c_slave_device_t *s = &i2c_slave->user_dev;
    uint32_t tx_fifo_rem;
    i2c_ll_get_txfifo_len(hal->dev, &tx_fifo_rem);
    uint8_t size = s->bufend - s->bufstart;
    if (size > tx_fifo_rem)
    {
        size = tx_fifo_rem;
    }
    if (size == 0)
    {
        // disable the interrupt, there is no data left to send
        i2c_ll_slave_disable_tx_it(hal->dev);
    }
    else
    {
        ESP_ERROR_CHECK(s->state == I2C_STATE_SEND ? ESP_OK : ESP_ERR_INVALID_STATE);
        i2c_ll_write_txfifo(hal->dev, s->buffer + s->bufstart, size);
        s->bufstart += size;
        i2c_ll_slave_clear_stretch(hal->dev);
        if (s->bufstart == s->bufend) // no more data to send
            i2c_ll_slave_disable_tx_it(hal->dev);
    }
}

static IRAM_ATTR void s_i2c_handle_clock_stretch(i2c_slave_dev_private_t *i2c_slave)
{
    i2c_hal_context_t *hal = &i2c_slave->hal;
    int stretch_cause = hal->dev->sr.stretch_cause;
    i2c_slave_device_t *s = &i2c_slave->user_dev;
    // esp_rom_printf("stretch cause is %d\n", stretch_cause);
    // esp_rom_printf("buffer length is %d\n", hal->dev->status_reg.tx_fifo_cnt);
    switch (stretch_cause)
    {
    case I2C_SLAVE_STRETCH_CAUSE_ADDRESS_MATCH:
        // start of a send. Receive any lingering data in the rx buffer, and call the callback.
        s_i2c_handle_rx_fifo_wm(i2c_slave);
        if (s->state == I2C_STATE_RECV)
        {
            // transaction in progress, but the master has re-addressed us, let the callback know.
            i2c_slave->callback(&i2c_slave->user_dev, I2C_CALLBACK_REPEAT_START);
        }
        // reset the buffer and fifos for the next transaction
        s->state = I2C_STATE_SEND;
        s_i2c_reset_buffer(i2c_slave);
        // tell the callback that we need some data.
        i2c_slave->callback(s, I2C_CALLBACK_SEND_DATA);
        // esp_rom_printf("here, receive mode is %d\n", hal->dev->status_reg.slave_rw);
        break;
    case I2C_SLAVE_STRETCH_CAUSE_RX_FULL:
    case I2C_SLAVE_STRETCH_CAUSE_SENDING_ACK:
    default:
        // clear the condition, we can't do anything at this point
        i2c_ll_slave_clear_stretch(hal->dev);
        break;
    case I2C_SLAVE_STRETCH_CAUSE_TX_EMPTY:
        // tell the callback we need more data
        i2c_slave->callback(s, I2C_CALLBACK_SEND_DATA);
        break;
    }
}

static IRAM_ATTR void s_slave_fifo_isr_handler(uint32_t int_mask, i2c_slave_dev_private_t *i2c_slave)
{
    if (int_mask & I2C_INTR_STRETCH)
    {
        s_i2c_handle_clock_stretch(i2c_slave);
    }
    if (int_mask & I2C_INTR_SLV_RXFIFO_WM)
    {
        s_i2c_handle_rx_fifo_wm(i2c_slave);
    }
    if (int_mask & I2C_INTR_SLV_COMPLETE)
    {
        s_i2c_handle_complete(i2c_slave);
    }
    if (int_mask & I2C_INTR_SLV_TXFIFO_WM)
    {
        s_i2c_handle_tx_fifo_wm(i2c_slave);
    }
}

static IRAM_ATTR void s_slave_isr_handle_default(void *arg)
{
    i2c_slave_dev_private_t *i2c_slave = (i2c_slave_dev_private_t *)arg;
    i2c_hal_context_t *hal = &i2c_slave->hal;
    uint32_t int_mask = 0;

    i2c_ll_get_intr_mask(hal->dev, &int_mask);
    // esp_rom_printf("int mask: %08lx\n", int_mask);
    // esp_rom_printf("rx buffer length is %d\n", hal->dev->status_reg.rx_fifo_cnt);
    // esp_rom_printf("slave addressed is %d\n", hal->dev->status_reg.slave_addressed);
    // esp_rom_printf("rw is %d\n", hal->dev->status_reg.slave_rw);
    if (int_mask == 0)
    {
        return;
    }

    s_slave_fifo_isr_handler(int_mask, i2c_slave);
    // clear the interrupts *after* handling.
    i2c_ll_clear_intr_mask(hal->dev, int_mask);
}

esp_err_t i2c_slave_new(i2c_slave_config_t *config, i2c_slave_device_t **result)
{
    // determine the port opened
    ESP_RETURN_ON_FALSE(config && result, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(config->gpio_sda) && GPIO_IS_VALID_GPIO(config->gpio_scl), ESP_ERR_INVALID_ARG, TAG, "invalid SDA/SCL pin number");
    i2c_slave_dev_private_t *dev = NULL;
    for (int i = 0; i < 2; ++i)
    {
        if (!i2cdev[i].allocated)
        {
            dev = &i2cdev[i];
            dev->allocated = true;
        }
    }
    // dev = heap_caps_calloc(1, sizeof(i2c_slave_dev_private_t), MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_STATE, TAG, "All slave drivers allocated!");
    dev->callback = config->callback;
    dev->scl = config->gpio_scl;
    dev->sda = config->gpio_sda;
    dev->portNum = config->i2c_port;
    dev->user_dev.bufstart = dev->user_dev.bufend = 0;
    // initialize the registers
    PERIPH_RCC_ATOMIC()
    {
        i2c_ll_enable_bus_clock(dev->portNum, true);
        i2c_ll_reset_register(dev->portNum);
    }

    // set up the hal
    i2c_hal_context_t *hal = &dev->hal;
    i2c_hal_init(hal, dev->portNum);
    // enable the GPIO config
    ESP_RETURN_ON_ERROR(s_hp_i2c_pins_config(dev), TAG, "Unable to set up pins");
    // set up the interrupt
    uint32_t isr_flags = (ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_LOWMED);
    uint32_t isr_mask = (I2C_TRANS_COMPLETE_INT_ENA_M |
                         I2C_SLAVE_STRETCH_INT_ENA_M |
                         I2C_RXFIFO_WM_INT_ENA_M |
                         I2C_TXFIFO_WM_INT_ENA_M);
    ESP_RETURN_ON_ERROR(esp_intr_alloc_intrstatus(i2c_periph_signal[dev->portNum].irq, isr_flags, (uint32_t)i2c_ll_get_interrupt_status_reg(hal->dev), isr_mask, s_slave_isr_handle_default, dev, &dev->intr), TAG, "Unable to create interrupt");
    i2c_ll_clear_intr_mask(hal->dev, isr_mask);
    i2c_hal_slave_init(hal);
    i2c_ll_set_source_clk(hal->dev, 4);
    i2c_ll_set_slave_addr(hal->dev, config->address, false);
    i2c_ll_set_txfifo_empty_thr(hal->dev, SOC_I2C_FIFO_LEN / 2);
    i2c_ll_set_rxfifo_full_thr(hal->dev, SOC_I2C_FIFO_LEN / 2);
    i2c_ll_set_sda_timing(hal->dev, 10, 10);
    i2c_ll_set_tout(hal->dev, 32000);

    // enable interrupts for stretch and receiveing, those always stay enabled.
    i2c_ll_slave_enable_scl_stretch(hal->dev, true);
    hal->dev->scl_stretch_conf.stretch_protect_num = 500;

    i2c_ll_slave_tx_auto_start_en(hal->dev, true);
    i2c_ll_slave_enable_rx_it(hal->dev);
    i2c_ll_enable_intr_mask(hal->dev, I2C_SLAVE_STRETCH_INT_ENA_M);

    i2c_ll_update(hal->dev);
    *result = &dev->user_dev;
    return ESP_OK;
}

IRAM_ATTR esp_err_t i2c_slave_send_data(i2c_slave_device_t *dev, uint8_t *buf, uint8_t len)
{
    // write the data to the buffer
    ESP_RETURN_ON_FALSE_ISR(dev->state == I2C_STATE_SEND, ESP_ERR_INVALID_STATE, TAG, "Trying to send while not in send state!");
    i2c_slave_dev_private_t *i2c_slave = (i2c_slave_dev_private_t *)dev;
    i2c_hal_context_t *hal = &i2c_slave->hal;
    len = MIN(len, sizeof(dev->buffer) - dev->bufend);
    memcpy(dev->buffer + dev->bufend, buf, len);
    dev->bufend += len;
    if (len > 0)
    {
        // enable the tx interrupt so data gets copied into the fifo
        i2c_ll_slave_enable_tx_it(hal->dev);
    }
    return ESP_OK;
}

IRAM_ATTR esp_err_t i2c_slave_del(i2c_slave_device_t *dev)
{
    i2c_slave_dev_private_t *i2c_slave = (i2c_slave_dev_private_t *)dev;
    ESP_RETURN_ON_FALSE(i2c_slave, ESP_ERR_INVALID_ARG, TAG, "invalid slave handle");
    ESP_RETURN_ON_FALSE(i2c_slave->allocated, ESP_ERR_INVALID_ARG, TAG, "Trying to deallocate unallocated slave");
    if (i2c_slave->allocated)
    {
        i2c_ll_disable_intr_mask(i2c_slave->hal.dev,
                                 I2C_TRANS_COMPLETE_INT_ENA_M |
                                     I2C_SLAVE_STRETCH_INT_ENA_M |
                                     I2C_RXFIFO_WM_INT_ENA_M |
                                     I2C_TXFIFO_WM_INT_ENA_M);
        ESP_RETURN_ON_ERROR(esp_intr_free(i2c_slave->intr), TAG, "delete interrupt service failed");
        PERIPH_RCC_ATOMIC()
        {
            i2c_ll_enable_bus_clock(i2c_slave->portNum, false);
        }
        i2c_slave_dev_private_t blank = {0};
        *i2c_slave = blank;
        // NOTE: pins are not cleared in the normal i2c driver, so we don't do it here.
    }
    return ESP_OK;
}