/*
 * Copyright (C) 2024 HTotoo
 *
 * This file is part of ESP32-Portapack.
 *
 * For additional license information, see the LICENSE file.
 */

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <esp_http_server.h>
#include <freertos/task.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_spiffs.h"

#include "spi_flash_mmap.h"
#include "led.h"

extern bool write_usb(const uint8_t *data, size_t len, bool mute, bool buffer);

static bool usbConnected = false; // to cache usb connected messages, so when got a query, can reply with it.

static httpd_handle_t server = NULL;

#define INDEX_HTML_PATH "/spiffs/index.html"
#define SETUP_HTML_PATH "/spiffs/setup.html"
#define OTA_HTML_PATH "/spiffs/ota.html"

// to show in setup.html, it is from wifim.h
extern char wifiAPSSID[64];
extern char wifiAPPASS[64];
extern char wifiStaSSID[64];
extern char wifiStaPASS[64];
extern char wifiHostName[64];

static char setup_html_out[3000];
extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");
extern const char setup_start[] asm("_binary_setup_html_start");
extern const char setup_end[] asm("_binary_setup_html_end");
extern const char ota_start[] asm("_binary_ota_html_start");
extern const char ota_end[] asm("_binary_ota_html_end");

extern uint32_t i2c_pp_connected;

// root / get handler.
static esp_err_t get_req_handler(httpd_req_t *req)
{
  const uint32_t index_len = index_end - index_start;
  int response = httpd_resp_send(req, index_start, index_len);
  return response;
}

/// setup.html get handler
static esp_err_t get_req_handler_setup(httpd_req_t *req)
{
  snprintf(setup_html_out, sizeof(setup_html_out), setup_start, wifiHostName, wifiAPSSID, wifiAPPASS, wifiStaSSID, wifiStaPASS, LedFeedback::get_brightness(), declinationAngle, gps_baud);
  int response = httpd_resp_send(req, setup_html_out, HTTPD_RESP_USE_STRLEN);
  return response;
}
/// setup.html get handler
static esp_err_t get_req_handler_ota(httpd_req_t *req)
{
  const uint32_t ota_len = ota_end - ota_start;
  int response = httpd_resp_send(req, ota_start, ota_len);
  return response;
}

// helper function to parse the POST variables
static int find_post_value(char *key, char *parameter, char *value)
{
  // char * addr1;
  char *addr1 = strstr(parameter, key);
  if (addr1 == NULL)
    return 0;
  ESP_LOGD(TAG, "addr1=%s", addr1);

  char *addr2 = addr1 + strlen(key);
  ESP_LOGD(TAG, "addr2=[%s]", addr2);

  char *addr3 = strstr(addr2, "&");
  ESP_LOGD(TAG, "addr3=%p", addr3);
  if (addr3 == NULL)
  {
    strcpy(value, addr2);
  }
  else
  {
    int length = addr3 - addr2;
    ESP_LOGD(TAG, "addr2=%p addr3=%p length=%d", addr2, addr3, length);
    strncpy(value, addr2, length);
    value[length] = 0;
  }
  ESP_LOGI(TAG, "key=[%s] value=[%s]", key, value);
  return strlen(value);
}

// setup.html post handler. saves the config
static esp_err_t post_req_handler_setup(httpd_req_t *req)
{
  char *buf = (char *)malloc(req->content_len + 1);
  size_t off = 0;
  while (off < req->content_len)
  {
    /* Read data received in the request */
    int ret = httpd_req_recv(req, buf + off, req->content_len - off);
    if (ret <= 0)
    {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT)
      {
        httpd_resp_send_408(req);
      }
      free(buf);
      return ESP_FAIL;
    }
    off += ret;
    ESP_LOGI(TAG, "root_post_handler recv length %d", ret);
  }
  buf[off] = '\0';

  // parse rets
  char tmp[65] = {0};
  uint8_t changeMask = 1; // wifi

  if (find_post_value((char *)"wifiHostName=", buf, tmp) > 0)
    strcpy(wifiHostName, tmp);
  if (find_post_value((char *)"wifiAPSSID=", buf, tmp) > 0)
    strcpy(wifiAPSSID, tmp);
  if (find_post_value((char *)"wifiAPPASS=", buf, tmp) > 0)
    strcpy(wifiAPPASS, tmp);
  if (find_post_value((char *)"wifiStaSSID=", buf, tmp) > 0)
    strcpy(wifiStaSSID, tmp);
  if (find_post_value((char *)"wifiStaPASS=", buf, tmp) > 0)
    strcpy(wifiStaPASS, tmp);

  if (find_post_value((char *)"rgb_brightness=", buf, tmp) > 0)
  {
    uint8_t rgb_brightness = (uint8_t)atoi(tmp);
    if (rgb_brightness > 100)
      rgb_brightness = 100;
    LedFeedback::set_brightness(rgb_brightness);
    changeMask |= 2;
  }
  if (find_post_value((char *)"gps_baud=", buf, tmp) > 0)
  {
    uint32_t gps_tmp = (uint32_t)atoi(tmp);
    if (gps_tmp == 1200 || gps_tmp == 2400 || gps_tmp == 4800 || gps_tmp == 9600 || gps_tmp == 14400 || gps_tmp == 19200 || gps_tmp == 38400 || gps_tmp == 57600 || gps_tmp == 115200)
    {
      gps_baud = gps_tmp;
      changeMask |= 2;
    }
  }
  if (find_post_value((char *)"declinationAngle=", buf, tmp) > 0)
  {
    for (int i = 0; tmp[i] != '\0'; i++) // replace international stuff
    {
      if (i > 64)
        break;
      if (tmp[i] == ',')
      {
        tmp[i] = '.';
      }
    }
    declinationAngle = atof(tmp);
    changeMask |= 4;
  }

  if ((changeMask & 1) == 1)
    save_config_wifi();
  if ((changeMask & 2) == 2)
    save_config_misc();
  if ((changeMask & 4) == 4)
    save_config_orientation();

  free(buf);
  /* Redirect onto root  */
  // todo should reboot, to apply new settings? pp would crash then, so keep it manual
  httpd_resp_set_status(req, "303 See Other");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_set_hdr(req, "Connection", "close");
  httpd_resp_sendstr(req, "post successfully");
  return ESP_OK;
}

// send data to all ws clients.
bool ws_sendall(uint8_t *data, size_t len)
{
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = (uint8_t *)data;
  ws_pkt.len = len;
  ws_pkt.type = HTTPD_WS_TYPE_BINARY;

  static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
  size_t fds = max_clients;
  int client_fds[max_clients];

  esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);

  if (ret != ESP_OK)
  {
    return false;
  }

  for (int i = 0; i < fds; i++)
  {
    int client_info = httpd_ws_get_fd_info(server, client_fds[i]);
    if (client_info == HTTPD_WS_CLIENT_WEBSOCKET)
    {
      httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
    }
  }
  return true;
}

esp_err_t update_post_handler(httpd_req_t *req)
{
  char buf[1000];
  esp_ota_handle_t ota_handle;
  int remaining = req->content_len;

  const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
  ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

  while (remaining > 0)
  {
    int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));

    // Timeout Error: Just retry
    if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
    {
      continue;

      // Serious Error: Abort OTA
    }
    else if (recv_len <= 0)
    {
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
      return ESP_FAIL;
    }

    // Successful Upload: Flash firmware chunk
    if (esp_ota_write(ota_handle, (const void *)buf, recv_len) != ESP_OK)
    {
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
      return ESP_FAIL;
    }

    remaining -= recv_len;
  }

  // Validate and switch to new OTA image and reboot
  if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK)
  {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
    return ESP_FAIL;
  }

  httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");

  vTaskDelay(500 / portTICK_PERIOD_MS);
  esp_restart();

  return ESP_OK;
}

// send to ws clients, the pp disconnected
void ws_notify_dc()
{
  usbConnected = false;
  char *data = (char *)"#$##$$#USB_DC\r\n";
  ws_sendall((uint8_t *)data, 15);
}
// send to ws clients, the pp connected
void ws_notify_cc()
{
  usbConnected = true;
  char *data = (char *)"#$##$$#USB_CC\r\n";
  ws_sendall((uint8_t *)data, 15);
}
void ws_notify_dc_i2c()
{
  char *data = (char *)"#$##$$#I2C_DC\r\n";
  ws_sendall((uint8_t *)data, 15);
}
// send to ws clients, the pp connected
void ws_notify_cc_i2c()
{
  char *data = (char *)"#$##$$#I2C_CC\r\n";
  ws_sendall((uint8_t *)data, 15);
}

// websocket handler
static esp_err_t handle_ws_req(httpd_req_t *req)
{
  if (req->method == HTTP_GET)
  {
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  uint8_t *buf = NULL;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
    return ret;
  }

  if (ws_pkt.len)
  {
    buf = (uint8_t *)calloc(1, ws_pkt.len + 1);
    if (buf == NULL)
    {
      ESP_LOGE(TAG, "Failed to calloc memory for buf");
      return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK)
    {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
      free(buf);
      return ret;
    }
    if (strcmp((const char *)ws_pkt.payload, "#$##$$#GETUSBSTATE\r\n") == 0)
    { // parse here, since we shouldn't sent it to pp
      if (usbConnected)
        ws_notify_cc();
      else if (i2c_pp_connected > 0)
        ws_notify_cc_i2c();
      else
        ws_notify_dc();
      return ESP_OK;
    }
    write_usb(ws_pkt.payload, ws_pkt.len, false, true);
    free(buf);
  }
  return ESP_OK;
}

// config web server part
static httpd_handle_t setup_websocket_server(void)
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_open_sockets = 10;

  httpd_uri_t uri_get = {.uri = "/",
                         .method = HTTP_GET,
                         .handler = get_req_handler,
                         .user_ctx = NULL,
                         .is_websocket = false,
                         .handle_ws_control_frames = false,
                         .supported_subprotocol = NULL};
  httpd_uri_t uri_postsetup = {.uri = "/setup.html",
                               .method = HTTP_POST,
                               .handler = post_req_handler_setup,
                               .user_ctx = NULL,
                               .is_websocket = false,
                               .handle_ws_control_frames = false,
                               .supported_subprotocol = NULL};
  httpd_uri_t uri_getsetup = {.uri = "/setup.html",
                              .method = HTTP_GET,
                              .handler = get_req_handler_setup,
                              .user_ctx = NULL,
                              .is_websocket = false,
                              .handle_ws_control_frames = false,
                              .supported_subprotocol = NULL};
  httpd_uri_t uri_getota = {.uri = "/ota.html",
                            .method = HTTP_GET,
                            .handler = get_req_handler_ota,
                            .user_ctx = NULL,
                            .is_websocket = false,
                            .handle_ws_control_frames = false,
                            .supported_subprotocol = NULL};
  httpd_uri_t update_post = {.uri = "/update",
                             .method = HTTP_POST,
                             .handler = update_post_handler,
                             .user_ctx = NULL,
                             .is_websocket = false,
                             .handle_ws_control_frames = false,
                             .supported_subprotocol = NULL};
  httpd_uri_t ws = {.uri = "/ws",
                    .method = HTTP_GET,
                    .handler = handle_ws_req,
                    .user_ctx = NULL,
                    .is_websocket = true,
                    .handle_ws_control_frames = false,
                    .supported_subprotocol = NULL};

  if (httpd_start(&server, &config) == ESP_OK)
  {
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_getsetup);
    httpd_register_uri_handler(server, &uri_postsetup);
    httpd_register_uri_handler(server, &uri_getota);
    httpd_register_uri_handler(server, &update_post);
    httpd_register_uri_handler(server, &ws);
  }

  return server;
}

// start web server, set it up,..
void init_httpd()
{
  server = setup_websocket_server();
}
