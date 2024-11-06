
/*
 * Copyright (C) 2024 HTotoo
 *
 * This file is part of ESP32-Portapack.
 *
 * For additional license information, see the LICENSE file.
 */

#ifndef PPSHELLCOMM_H
#define PPSHELLCOMM_H

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"

typedef struct
{
    char buffer[1024];
    size_t len;
    bool muted;
} SendBuffer;

class PPShellComm {
   public:
    static void init();
    static bool write(const uint8_t* data, size_t len, bool mute, bool buffer);
    static bool write_blocking(const uint8_t* data, size_t len, bool mute, bool buffer);
    static bool wait_till_sending(uint32_t timeoutMs);
    static bool getInCommand() { return inCommand; }
    static bool getAnyConnected() { return cdc_dev != NULL; }
    static void set_data_rx_callback(bool (*callback)(const uint8_t* data, size_t data_len)) {
        data_rx_callback = callback;
    }

   private:
    static void usb_init();
    static void usb_lib_task(void* arg);
    static void usb_buffer_task(void* arg);
    static void tryconnectUsb(void* arg);
    static void handle_usb_event(const cdc_acm_host_dev_event_data_t* event, void* user_ctx);
    static bool handle_usb_rx(const uint8_t* data, size_t data_len, void* arg);
    static bool wait_till_usb_sending(uint32_t timeoutMs);
    static bool write_usb_blocking(const uint8_t* data, size_t len, bool mute, bool buffer);
    static bool write_usb(const uint8_t* data, size_t len, bool mute, bool buffer);
    static void searchPromptAdd(uint8_t ch);

    static bool usb_connected;
    static bool i2c_connected;
    static bool (*data_rx_callback)(const uint8_t* data, size_t data_len);

    static char* PROMPT;
    static char searchPrompt[5];
    static bool inCommand;  // sending / recv now to web
    static bool isMuted;    // reply not needed to ws until next prompt

    static SendBuffer send_buffer;
    static SemaphoreHandle_t send_block_sem;
    static SemaphoreHandle_t send_buffer_sem;
    static SemaphoreHandle_t device_disconnected_sem;

    static cdc_acm_dev_hdl_t cdc_dev;
};

#endif  // PPSHELLCOMM_H