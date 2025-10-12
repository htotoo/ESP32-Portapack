#include "ppshellcomm.h"

#define USB_DEVICE_VID (0x1D50)
#define USB_DEVICE_PID (0x6018)
#define TAG "PPShellComm"

void ws_notify_cc();
void ws_notify_dc();
bool ws_sendall(uint8_t* data, size_t len);

bool (*PPShellComm::data_rx_callback)(const uint8_t* data, size_t data_len) = nullptr;

bool PPShellComm::usb_connected = false;
bool PPShellComm::i2c_connected = false;

char* PPShellComm::PROMPT = (char*)"ch> ";
char PPShellComm::searchPrompt[5] = {0};
bool PPShellComm::inCommand = false;
bool PPShellComm::isMuted = false;

SendBuffer PPShellComm::send_buffer;
SemaphoreHandle_t PPShellComm::send_block_sem;
SemaphoreHandle_t PPShellComm::send_buffer_sem;
SemaphoreHandle_t PPShellComm::device_disconnected_sem;
cdc_acm_dev_hdl_t PPShellComm::cdc_dev = NULL;
QueueHandle_t PPShellComm::datain_queue;
I2CQueueMessage_t PPShellComm::tx;

void PPShellComm::init() {
    datain_queue = xQueueCreate(QUEUE_SIZE, sizeof(I2CQueueMessage_t));
    usb_init();
    // i2c rx handler init
    xTaskCreate(processi2c_queuein_task, "i2crxinth", 4096, xTaskGetCurrentTaskHandle(), 20, NULL);
}

bool PPShellComm::write(const uint8_t* data, size_t len, bool mute, bool buffer) {
    if (usb_connected) {
        return write_usb(data, len, mute, buffer);
    }
    if (i2c_connected) {
        tx.size = len;
        ESP_LOGE(TAG, "i2c write");
        if (len > 64) {
            ESP_LOGE(TAG, "Data too big for I2C");
            return false;
        }
        memcpy(tx.data, data, len);  // todo fix for real, buffered write
        inCommand = true;
        return true;
    }
    return false;
}
bool PPShellComm::write_blocking(const uint8_t* data, size_t len, bool mute, bool buffer) {
    if (usb_connected) {
        return write_usb_blocking(data, len, mute, buffer);
    }
    if (i2c_connected) {
        tx.size = len;
        if (len > 64) {
            ESP_LOGE(TAG, "Data too big for I2C");
            return false;
        }
        memcpy(tx.data, data, len);  // todo fix for real buffered blocking write
        return true;
    }
    return false;
}

bool PPShellComm::wait_till_sending(uint32_t timeoutMs) {
    if (usb_connected) {
        return wait_till_usb_sending(timeoutMs);
    }
    // todo i2c send wait, but hey, we should not send to i2c!!! check this
    return false;
}

void PPShellComm::processi2c_queuein_task(void* pvParameters) {
    I2CQueueMessage_t rx;
    while (1) {
        if (xQueueReceive(datain_queue, &rx, portMAX_DELAY)) {
            for (size_t i = 0; i < rx.size; ++i) {
                searchPromptAdd(rx.data[i]);
            }
            if (data_rx_callback)
                data_rx_callback(rx.data, rx.size);
        }
    }
}

void PPShellComm::usb_init() {
    device_disconnected_sem = xSemaphoreCreateBinary();
    send_block_sem = xSemaphoreCreateBinary();
    send_buffer_sem = xSemaphoreCreateBinary();
    assert(device_disconnected_sem);
    assert(send_block_sem);
    assert(send_buffer_sem);
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
        .enum_filter_cb = NULL,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), 20, NULL);
    assert(task_created == pdTRUE);
    BaseType_t task_buffer_created = xTaskCreate(usb_buffer_task, "usb_buffer", 4096, xTaskGetCurrentTaskHandle(), 20, NULL);
    assert(task_buffer_created == pdTRUE);
    ESP_LOGI(TAG, "Installing CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    BaseType_t task_created2 = xTaskCreate(tryconnectUsb, "tryconnectUsb", 4096, xTaskGetCurrentTaskHandle(), 20, NULL);
    assert(task_created2 == pdTRUE);
}

void PPShellComm::usb_lib_task(void* arg) {
    while (1) {
        // Start handling system events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            // ESP_LOGI(TAG, "USB: All devices freed");
            //  Continue handling USB events to allow device reconnection
        }
    }
}

void PPShellComm::usb_buffer_task(void* arg) {
    while (1) {
        xSemaphoreTake(send_buffer_sem, portMAX_DELAY);
        if (send_buffer.len >= 1) {
            // copy
            SendBuffer send_buffer_tmp = send_buffer;
            send_buffer.len = 0;
            xSemaphoreGive(send_buffer_sem);
            // send it to
            wait_till_usb_sending(5000);
            write_usb_blocking((const uint8_t*)(send_buffer_tmp.buffer), send_buffer_tmp.len, send_buffer_tmp.muted, false);
        } else {
            xSemaphoreGive(send_buffer_sem);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void PPShellComm::tryconnectUsb(void* arg) {
    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .event_cb = handle_usb_event,
        .data_cb = handle_usb_rx,
        .user_arg = NULL,
    };
    while (true) {
        // ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X...", USB_DEVICE_VID,
        // USB_DEVICE_PID);
        esp_err_t err = cdc_acm_host_open(USB_DEVICE_VID, USB_DEVICE_PID, 0,
                                          &dev_config, &cdc_dev);
        if (ESP_OK != err) {
            // ESP_LOGI(TAG, "Failed to open device");
            continue;
        }
        cdc_acm_line_coding_t line_coding;
        line_coding.dwDTERate = 115200;
        line_coding.bDataBits = 8;
        line_coding.bParityType = 0;
        line_coding.bCharFormat = 1;
        ESP_ERROR_CHECK(cdc_acm_host_line_coding_set(cdc_dev, &line_coding));
        ESP_LOGI(TAG,
                 "Line Set: Rate: %" PRIu32 ", Stop bits: %" PRIu8
                 ", Parity: %" PRIu8 ", Databits: %" PRIu8 "",
                 line_coding.dwDTERate, line_coding.bCharFormat,
                 line_coding.bParityType, line_coding.bDataBits);
        ESP_ERROR_CHECK(cdc_acm_host_set_control_line_state(cdc_dev, true, false));
        cdc_acm_host_desc_print(cdc_dev);
        inCommand = false;
        usb_connected = true;
        ws_notify_cc();
        xSemaphoreTake(device_disconnected_sem, portMAX_DELAY);
    }
}

void PPShellComm::handle_usb_event(const cdc_acm_host_dev_event_data_t* event, void* user_ctx) {
    switch (event->type) {
        case CDC_ACM_HOST_ERROR:
            ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %i",
                     event->data.error);
            break;
        case CDC_ACM_HOST_DEVICE_DISCONNECTED:
            ESP_LOGI(TAG, "Device suddenly disconnected");
            ESP_ERROR_CHECK(cdc_acm_host_close(event->data.cdc_hdl));
            xSemaphoreGive(device_disconnected_sem);
            inCommand = false;
            usb_connected = false;
            ws_notify_dc();
            break;
        case CDC_ACM_HOST_SERIAL_STATE:
            ESP_LOGI(TAG, "Serial state notif 0x%04X", event->data.serial_state.val);
            break;
        case CDC_ACM_HOST_NETWORK_CONNECTION:
        default:
            ESP_LOGW(TAG, "Unsupported CDC event: %i", event->type);
            break;
    }
}

bool PPShellComm::handle_usb_rx(const uint8_t* data, size_t data_len, void* arg) {
    // ESP_LOGI(TAG, "Data received");
    // ESP_LOG_BUFFER_HEXDUMP(TAG, data, data_len, ESP_LOG_INFO);
    for (size_t i = 0; i < data_len; ++i) {
        searchPromptAdd(data[i]);
    }
    if (!isMuted)
        if (data_rx_callback)
            data_rx_callback(data, data_len);
    return true;
}

void PPShellComm::searchPromptAdd(uint8_t ch) {
    // shift
    for (uint8_t i = 0; i < 3; ++i) {
        searchPrompt[i] = searchPrompt[i + 1];
    }
    searchPrompt[3] = ch;
    if (strncmp(PROMPT, searchPrompt, 4) == 0) {
        inCommand = false;
        xSemaphoreGive(send_block_sem);
    }
}

bool PPShellComm::write_usb(const uint8_t* data, size_t len, bool mute, bool buffer) {
    if (cdc_dev == NULL)
        return false;
    if (inCommand && buffer) {
        xSemaphoreTake(send_buffer_sem, 5000 / portTICK_PERIOD_MS);
        memcpy(send_buffer.buffer, data, len < 1024 ? len : 1024);
        xSemaphoreGive(send_buffer_sem);
    }
    isMuted = mute;
    inCommand = true;
    return cdc_acm_host_data_tx_blocking(cdc_dev, data, len, 30000) == ESP_OK;
}

bool PPShellComm::write_usb_blocking(const uint8_t* data, size_t len, bool mute, bool buffer) {
    if (!write_usb(data, len, mute, buffer))
        return false;
    return xSemaphoreTake(send_block_sem, 4000 / portTICK_PERIOD_MS);
}

bool PPShellComm::wait_till_usb_sending(uint32_t timeoutMs) {
    if (!inCommand)
        return true;
    return xSemaphoreTake(send_block_sem, timeoutMs / portTICK_PERIOD_MS) == pdTRUE;
}
