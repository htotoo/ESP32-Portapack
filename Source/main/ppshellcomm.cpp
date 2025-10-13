#include "ppshellcomm.h"

#define USB_DEVICE_VID (0x1D50)
#define USB_DEVICE_PID (0x6018)
#define TAG "PPShellComm"

void ws_notify_cc();
void ws_notify_dc();
bool ws_sendall(uint8_t* data, size_t len);

bool (*PPShellComm::data_rx_callback)(const uint8_t* data, size_t data_len) = nullptr;

bool PPShellComm::i2c_connected = false;

char* PPShellComm::PROMPT = (char*)"ch> ";
char PPShellComm::searchPrompt[5] = {0};
bool PPShellComm::inCommand = false;
bool PPShellComm::isMuted = false;

SendBuffer PPShellComm::send_buffer;
SemaphoreHandle_t PPShellComm::send_block_sem;
SemaphoreHandle_t PPShellComm::send_buffer_sem;
SemaphoreHandle_t PPShellComm::device_disconnected_sem;
QueueHandle_t PPShellComm::datain_queue;
I2CQueueMessage_t PPShellComm::tx;

void PPShellComm::init() {
    datain_queue = xQueueCreate(QUEUE_SIZE, sizeof(I2CQueueMessage_t));
    // i2c rx handler init
    xTaskCreate(processi2c_queuein_task, "i2crxinth", 4096, xTaskGetCurrentTaskHandle(), 20, NULL);
}

bool PPShellComm::write(const uint8_t* data, size_t len, bool mute, bool buffer) {
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
