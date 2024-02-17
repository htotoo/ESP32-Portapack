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

#define USB_DEVICE_VID (0x1D50)
#define USB_DEVICE_PID (0x6018)

typedef struct
{
  char buffer[1024];
  size_t len;
  bool muted;
} SendBuffer;

// to store the usb out data when there is ongoing internal tx (while send in progress and something comes in)
SendBuffer send_buffer;
static SemaphoreHandle_t send_block_sem;
static SemaphoreHandle_t send_buffer_sem;

cdc_acm_dev_hdl_t cdc_dev = NULL;
static SemaphoreHandle_t device_disconnected_sem;
bool inCommand = false; // sending / recv now to web
bool isMuted = false;   // reply not needed to ws until next prompt

bool getInCommand()
{
  return inCommand;
}

char *PROMPT = "ch> ";
char searchPrompt[5] = {0};

static void searchPromptAdd(uint8_t ch)
{
  // shift
  for (uint8_t i = 0; i < 3; ++i)
  {
    searchPrompt[i] = searchPrompt[i + 1];
  }
  searchPrompt[3] = ch;
  if (strncmp(PROMPT, searchPrompt, 4) == 0)
  {
    inCommand = false;
    xSemaphoreGive(send_block_sem);
  }
}

bool write_usb(const uint8_t *data, size_t len, bool mute, bool buffer)
{
  if (cdc_dev == NULL)
    return false;
  if (inCommand && buffer)
  {
    xSemaphoreTake(send_buffer_sem, 5000 / portTICK_PERIOD_MS);
    memcpy(send_buffer.buffer, data, len < 1024 ? len : 1024);
    xSemaphoreGive(send_buffer_sem);
  }
  isMuted = mute;
  inCommand = true;
  return cdc_acm_host_data_tx_blocking(cdc_dev, data, len, 30000) == ESP_OK;
}

bool write_usb_blocking(const uint8_t *data, size_t len, bool mute, bool buffer)
{
  if (!write_usb(data, len, mute, buffer))
    return false;
  return xSemaphoreTake(send_block_sem, 4000 / portTICK_PERIOD_MS);
}

void wait_till_usb_sending(uint32_t timeoutMs)
{
  if (!inCommand)
    return;
  xSemaphoreTake(send_block_sem, timeoutMs / portTICK_PERIOD_MS);
}

static bool handle_rx(const uint8_t *data, size_t data_len, void *arg)
{
  // ESP_LOGI(TAG, "Data received");
  // ESP_LOG_BUFFER_HEXDUMP(TAG, data, data_len, ESP_LOG_INFO);
  for (size_t i = 0; i < data_len; ++i)
  {
    searchPromptAdd(data[i]);
  }
  if (!isMuted)
    ws_sendall((uint8_t *)data, data_len);
  return true;
}

static void handle_event(const cdc_acm_host_dev_event_data_t *event,
                         void *user_ctx)
{
  switch (event->type)
  {
  case CDC_ACM_HOST_ERROR:
    ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %i",
             event->data.error);
    break;
  case CDC_ACM_HOST_DEVICE_DISCONNECTED:
    ESP_LOGI(TAG, "Device suddenly disconnected");
    ESP_ERROR_CHECK(cdc_acm_host_close(event->data.cdc_hdl));
    xSemaphoreGive(device_disconnected_sem);
    inCommand = false;
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

static void tryconnectUsb(void *arg)
{
  const cdc_acm_host_device_config_t dev_config = {
      .connection_timeout_ms = 1000,
      .out_buffer_size = 512,
      .in_buffer_size = 512,
      .user_arg = NULL,
      .event_cb = handle_event,
      .data_cb = handle_rx};
  while (true)
  {
    // ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X...", USB_DEVICE_VID,
    // USB_DEVICE_PID);
    esp_err_t err = cdc_acm_host_open(USB_DEVICE_VID, USB_DEVICE_PID, 0,
                                      &dev_config, &cdc_dev);
    if (ESP_OK != err)
    {
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
    ws_notify_cc();
    xSemaphoreTake(device_disconnected_sem, portMAX_DELAY);
  }
}

static void usb_buffer_task(void *arg)
{
  while (1)
  {
    xSemaphoreTake(send_buffer_sem, portMAX_DELAY);
    if (send_buffer.len >= 1)
    {
      // copy
      SendBuffer send_buffer_tmp = send_buffer;
      send_buffer.len = 0;
      xSemaphoreGive(send_buffer_sem);
      // send it to
      wait_till_usb_sending(5000);
      write_usb_blocking((const uint8_t *)(send_buffer_tmp.buffer), send_buffer_tmp.len, send_buffer_tmp.muted, false);
    }
    else
    {
      xSemaphoreGive(send_buffer_sem);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

static void usb_lib_task(void *arg)
{
  while (1)
  {
    // Start handling system events
    uint32_t event_flags;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
    {
      ESP_ERROR_CHECK(usb_host_device_free_all());
    }
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
    {
      // ESP_LOGI(TAG, "USB: All devices freed");
      //  Continue handling USB events to allow device reconnection
    }
  }
}

void usb_init()
{
  device_disconnected_sem = xSemaphoreCreateBinary();
  send_block_sem = xSemaphoreCreateBinary();
  send_buffer_sem = xSemaphoreCreateBinary();
  assert(device_disconnected_sem);
  assert(send_block_sem);
  assert(send_buffer_sem);
  usb_host_config_t host_config = {
      .skip_phy_setup = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
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