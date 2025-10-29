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
#include "wifim.h"
#include "led.h"
#include "ppshellcomm.h"
#include "pinconfig.h"
#include "pinconfig_html.h"

static httpd_handle_t server = NULL;
static bool disable_esp_async = false;  // for example while in file transfer mode, don't send anything else

#define INDEX_HTML_PATH "/spiffs/index.html"
#define SETUP_HTML_PATH "/spiffs/setup.html"
#define SETUP_CSS_PATH "/spiffs/setup.css"
#define OTA_HTML_PATH "/spiffs/ota.html"

static char setup_html_out[3300];
extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");
extern const char setup_start[] asm("_binary_setup_html_start");
extern const char setup_end[] asm("_binary_setup_html_end");
extern const char setupcss_start[] asm("_binary_setup_css_start");
extern const char setupcss_end[] asm("_binary_setup_css_end");
extern const char ota_start[] asm("_binary_ota_html_start");
extern const char ota_end[] asm("_binary_ota_html_end");

extern PinConfig pinConfig;

static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return 0;
}

// Function to decode a URL-encoded string
std::string url_decode(const std::string encoded_string) {
    std::string decoded_string;
    for (size_t i = 0; i < encoded_string.length(); ++i) {
        if (encoded_string[i] == '%' && i + 2 < encoded_string.length() && isxdigit(encoded_string[i + 1]) && isxdigit(encoded_string[i + 2])) {
            int high = hex_to_int(encoded_string[++i]);
            int low = hex_to_int(encoded_string[++i]);
            decoded_string += static_cast<char>((high << 4) | low);
        } else if (encoded_string[i] == '+') {
            decoded_string += ' ';
        } else {
            decoded_string += encoded_string[i];
        }
    }
    return decoded_string;
}

/// pinconfig.html get handler
static esp_err_t get_req_handler_pinconfig(httpd_req_t* req) {
    // A single, small buffer for all variable-to-string conversions
    char val_buf[20];  // Plenty for a 32-bit integer

    // Send part 1
    httpd_resp_send_chunk(req, PINCONFIG_HTML_PART1, HTTPD_RESP_USE_STRLEN);

    // Send variable 1 (ledRgbPin)
    snprintf(val_buf, sizeof(val_buf), "%ld", pinConfig.LedRgbPin());
    httpd_resp_send_chunk(req, val_buf, HTTPD_RESP_USE_STRLEN);

    // Send part 2
    httpd_resp_send_chunk(req, PINCONFIG_HTML_PART2, HTTPD_RESP_USE_STRLEN);

    // Send variable 2 (gpsRxPin)
    snprintf(val_buf, sizeof(val_buf), "%ld", pinConfig.GpsRxPin());
    httpd_resp_send_chunk(req, val_buf, HTTPD_RESP_USE_STRLEN);

    // Send part 3
    httpd_resp_send_chunk(req, PINCONFIG_HTML_PART3, HTTPD_RESP_USE_STRLEN);

    // Send variable 3 (i2cSdaPin)
    snprintf(val_buf, sizeof(val_buf), "%ld", pinConfig.I2cSdaPin());
    httpd_resp_send_chunk(req, val_buf, HTTPD_RESP_USE_STRLEN);

    // Send part 4
    httpd_resp_send_chunk(req, PINCONFIG_HTML_PART4, HTTPD_RESP_USE_STRLEN);

    // Send variable 4 (i2cSclPin)
    snprintf(val_buf, sizeof(val_buf), "%ld", pinConfig.I2cSclPin());
    httpd_resp_send_chunk(req, val_buf, HTTPD_RESP_USE_STRLEN);

    // Send part 5
    httpd_resp_send_chunk(req, PINCONFIG_HTML_PART5, HTTPD_RESP_USE_STRLEN);

    // Send variable 5 (irRxPin)
    snprintf(val_buf, sizeof(val_buf), "%ld", pinConfig.IrRxPin());
    httpd_resp_send_chunk(req, val_buf, HTTPD_RESP_USE_STRLEN);

    // Send part 6
    httpd_resp_send_chunk(req, PINCONFIG_HTML_PART6, HTTPD_RESP_USE_STRLEN);

    // Send variable 6 (irTxPin)
    snprintf(val_buf, sizeof(val_buf), "%ld", pinConfig.IrTxPin());
    httpd_resp_send_chunk(req, val_buf, HTTPD_RESP_USE_STRLEN);

    // Send part 7
    httpd_resp_send_chunk(req, PINCONFIG_HTML_PART7, HTTPD_RESP_USE_STRLEN);

    // Send variable 7 (i2cSdaSlavePin)
    snprintf(val_buf, sizeof(val_buf), "%ld", pinConfig.I2cSdaSlavePin());
    httpd_resp_send_chunk(req, val_buf, HTTPD_RESP_USE_STRLEN);

    // Send part 8
    httpd_resp_send_chunk(req, PINCONFIG_HTML_PART8, HTTPD_RESP_USE_STRLEN);

    // Send variable 8 (i2cSclSlavePin)
    snprintf(val_buf, sizeof(val_buf), "%ld", pinConfig.I2cSclSlavePin());
    httpd_resp_send_chunk(req, val_buf, HTTPD_RESP_USE_STRLEN);

    // Send part 9 (the rest of the file)
    httpd_resp_send_chunk(req, PINCONFIG_HTML_PART9, HTTPD_RESP_USE_STRLEN);

    // Send the final, empty chunk to finish the response
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

// root / get handler.
static esp_err_t get_req_handler(httpd_req_t* req) {
    // pinConfig.debugPrint();
    if (pinConfig.isPinsOk()) {
        const uint32_t index_len = index_end - index_start;
        int response = httpd_resp_send(req, index_start, index_len);
        return response;
    }
    // pins not ok, show the pinconfig.html
    return get_req_handler_pinconfig(req);
}

static esp_err_t get_req_handler_setupcss(httpd_req_t* req) {
    const uint32_t setupcss_len = setupcss_end - setupcss_start;
    httpd_resp_set_type(req, "text/css");
    int response = httpd_resp_send(req, setupcss_start, setupcss_len);
    return response;
}

/// setup.html get handler
static esp_err_t get_req_handler_setup(httpd_req_t* req) {
    snprintf(setup_html_out, sizeof(setup_html_out), setup_start, WifiM::wifiHostName, WifiM::wifiAPSSID, WifiM::wifiAPPASS, WifiM::wifiStaSSID, WifiM::wifiStaPASS, LedFeedback::get_brightness(), declinationAngle, gps_baud);
    int response = httpd_resp_send(req, setup_html_out, HTTPD_RESP_USE_STRLEN);
    return response;
}
/// ota.html get handler
static esp_err_t get_req_handler_ota(httpd_req_t* req) {
    const uint32_t ota_len = ota_end - ota_start;
    int response = httpd_resp_send(req, ota_start, ota_len);
    return response;
}

// helper function to parse the POST variables
static int find_post_value(char* key, char* parameter, char* value) {
    // char * addr1;
    char* addr1 = strstr(parameter, key);
    if (addr1 == NULL)
        return 0;
    ESP_LOGD("WEBS", "addr1=%s", addr1);

    char* addr2 = addr1 + strlen(key);
    ESP_LOGD("WEBS", "addr2=[%s]", addr2);

    char* addr3 = strstr(addr2, "&");
    ESP_LOGD("WEBS", "addr3=%p", addr3);
    if (addr3 == NULL) {
        strcpy(value, addr2);
    } else {
        int length = addr3 - addr2;
        ESP_LOGD("WEBS", "addr2=%p addr3=%p length=%d", addr2, addr3, length);
        strncpy(value, addr2, length);
        value[length] = 0;
    }
    ESP_LOGI("WEBS", "key=[%s] value=[%s]", key, value);
    return strlen(value);
}

// setup.html post handler. saves the config
static esp_err_t post_req_handler_setup(httpd_req_t* req) {
    char* buf = (char*)malloc(req->content_len + 1);
    size_t off = 0;
    while (off < req->content_len) {
        /* Read data received in the request */
        int ret = httpd_req_recv(req, buf + off, req->content_len - off);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            free(buf);
            return ESP_FAIL;
        }
        off += ret;
        ESP_LOGI("WEBS", "root_post_handler recv length %d", ret);
    }
    buf[off] = '\0';

    // parse rets
    char tmp[65] = {0};
    uint8_t changeMask = 1;  // wifi

    if (find_post_value((char*)"wifiHostName=", buf, tmp) > 0) {
        std::string tmp2 = url_decode(tmp);
        if (tmp2.length() > 63) {
            tmp2.resize(63);
        }
        strcpy(WifiM::wifiHostName, tmp2.data());
    }
    if (find_post_value((char*)"wifiAPSSID=", buf, tmp) > 0) {
        std::string tmp2 = url_decode(tmp);
        if (tmp2.length() > 63) {
            tmp2.resize(63);
        }
        strcpy(WifiM::wifiAPSSID, tmp2.data());
    }
    if (find_post_value((char*)"wifiAPPASS=", buf, tmp) > 0) {
        std::string tmp2 = url_decode(tmp);
        if (tmp2.length() > 63) {
            tmp2.resize(63);
        }
        strcpy(WifiM::wifiAPPASS, tmp2.data());
    }
    if (find_post_value((char*)"wifiStaSSID=", buf, tmp) > 0) {
        std::string tmp2 = url_decode(tmp);
        if (tmp2.length() > 63) {
            tmp2.resize(63);
        }
        strcpy(WifiM::wifiStaSSID, tmp2.data());
    }
    if (find_post_value((char*)"wifiStaPASS=", buf, tmp) > 0) {
        std::string tmp2 = url_decode(tmp);
        if (tmp2.length() > 63) {
            tmp2.resize(63);
        }
        strcpy(WifiM::wifiStaPASS, tmp2.data());
    }
    // ESP_LOGI("WEBS", "wifiHostName=[%s] wifiAPSSID=[%s] wifiAPPASS=[%s] wifiStaSSID=[%s] wifiStaPASS=[%s]", WifiM::wifiHostName, WifiM::wifiAPSSID, WifiM::wifiAPPASS, WifiM::wifiStaSSID, WifiM::wifiStaPASS);

    if (find_post_value((char*)"rgb_brightness=", buf, tmp) > 0) {
        uint8_t rgb_brightness = (uint8_t)atoi(tmp);
        if (rgb_brightness > 100)
            rgb_brightness = 100;
        LedFeedback::set_brightness(rgb_brightness);
        changeMask |= 2;
    }
    if (find_post_value((char*)"gps_baud=", buf, tmp) > 0) {
        uint32_t gps_tmp = (uint32_t)atoi(tmp);
        if (gps_tmp == 1200 || gps_tmp == 2400 || gps_tmp == 4800 || gps_tmp == 9600 || gps_tmp == 14400 || gps_tmp == 19200 || gps_tmp == 38400 || gps_tmp == 57600 || gps_tmp == 115200) {
            gps_baud = gps_tmp;
            changeMask |= 2;
        }
    }
    if (find_post_value((char*)"declinationAngle=", buf, tmp) > 0) {
        for (int i = 0; tmp[i] != '\0'; i++)  // replace international stuff
        {
            if (i > 64)
                break;
            if (tmp[i] == ',') {
                tmp[i] = '.';
            }
        }
        declinationAngle = atof(tmp);
        changeMask |= 4;
    }

    if ((changeMask & 1) == 1) {
        WifiM::save_config_wifi();
        WifiM::config_wifi_apsta();
    }
    if ((changeMask & 2) == 2)
        save_config_misc();
    if ((changeMask & 4) == 4)
        save_config_orientation();

    free(buf);
    /* Redirect onto root  */
    // todo should reboot, to apply new settings?
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, "post successfully");
    return ESP_OK;
}

// pinconfig.html post handler. saves the config
static esp_err_t post_req_handler_pinconfig(httpd_req_t* req) {
    char* buf = (char*)malloc(req->content_len + 1);
    if (buf == NULL) {
        ESP_LOGE("WEBS", "Failed to malloc memory for POST buffer");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    size_t off = 0;
    while (off < req->content_len) {
        int ret = httpd_req_recv(req, buf + off, req->content_len - off);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            free(buf);
            return ESP_FAIL;
        }
        off += ret;
    }
    buf[off] = '\0';
    char tmp[65] = {0};
    bool changed = false;
    // Load the *current* pin values first.
    // This ensures that if a field is missing from the POST, the old value is kept.
    int32_t ledRgb = pinConfig.LedRgbPin();
    int32_t gpsRx = pinConfig.GpsRxPin();
    int32_t i2cSda = pinConfig.I2cSdaPin();
    int32_t i2cScl = pinConfig.I2cSclPin();
    int32_t irRx = pinConfig.IrRxPin();
    int32_t irTx = pinConfig.IrTxPin();
    int32_t i2cSdaSlave = pinConfig.I2cSdaSlavePin();
    int32_t i2cSclSlave = pinConfig.I2cSclSlavePin();
    if (find_post_value((char*)"ledRgbPin=", buf, tmp) > 0) {
        ledRgb = (int32_t)atoi(tmp);
        changed = true;
    }
    if (find_post_value((char*)"gpsRxPin=", buf, tmp) > 0) {
        gpsRx = (int32_t)atoi(tmp);
        changed = true;
    }
    if (find_post_value((char*)"i2cSdaPin=", buf, tmp) > 0) {
        i2cSda = (int32_t)atoi(tmp);
        changed = true;
    }
    if (find_post_value((char*)"i2cSclPin=", buf, tmp) > 0) {
        i2cScl = (int32_t)atoi(tmp);
        changed = true;
    }
    if (find_post_value((char*)"irRxPin=", buf, tmp) > 0) {
        irRx = (int32_t)atoi(tmp);
        changed = true;
    }
    if (find_post_value((char*)"irTxPin=", buf, tmp) > 0) {
        irTx = (int32_t)atoi(tmp);
        changed = true;
    }
    if (find_post_value((char*)"i2cSdaSlavePin=", buf, tmp) > 0) {
        i2cSdaSlave = (int32_t)atoi(tmp);
        changed = true;
    }
    if (find_post_value((char*)"i2cSclSlavePin=", buf, tmp) > 0) {
        i2cSclSlave = (int32_t)atoi(tmp);
        changed = true;
    }
    if (changed) {
        ESP_LOGI("WEBS", "Saving new PinConfig to NVS.");
        pinConfig.setPins(ledRgb, gpsRx, i2cSda, i2cScl, irRx, irTx, i2cSdaSlave, i2cSclSlave);
        pinConfig.saveToNvs();
    } else {
        ESP_LOGI("WEBS", "No pin changes detected.");
    }
    free(buf);
    // Redirect back to the root page, just like the setup handler
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");  // Redirects to root
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, "Pin config saved successfully");
    return ESP_OK;
}

// send data to all ws clients.
bool ws_sendall(uint8_t* data, size_t len, bool asyncmsg = false) {
    if (disable_esp_async && asyncmsg) {
        return false;
    }
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = len;
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;

    static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[max_clients];

    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);

    if (ret != ESP_OK) {
        return false;
    }

    for (int i = 0; i < fds; i++) {
        int client_info = httpd_ws_get_fd_info(server, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
        }
    }
    return true;
}

esp_err_t update_post_handler(httpd_req_t* req) {
    char buf[1000];
    esp_ota_handle_t ota_handle;
    int remaining = req->content_len;

    const esp_partition_t* ota_partition = esp_ota_get_next_update_partition(NULL);
    ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));

        // Timeout Error: Just retry
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;

            // Serious Error: Abort OTA
        } else if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            return ESP_FAIL;
        }

        // Successful Upload: Flash firmware chunk
        if (esp_ota_write(ota_handle, (const void*)buf, recv_len) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
            return ESP_FAIL;
        }

        remaining -= recv_len;
    }

    // Validate and switch to new OTA image and reboot
    if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");

    vTaskDelay(500 / portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}

// send to ws clients, the pp disconnected
void ws_notify_dc_i2c() {
    char* data = (char*)"#$##$$#I2C_DC\r\n";
    ws_sendall((uint8_t*)data, 15);
}
// send to ws clients, the pp connected
void ws_notify_cc_i2c() {
    char* data = (char*)"#$##$$#I2C_CC\r\n";
    ws_sendall((uint8_t*)data, 15);
}

// websocket handler
static esp_err_t handle_ws_req(httpd_req_t* req) {
    if (req->method == HTTP_GET) {
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t* buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE("WEBS", "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }

    if (ws_pkt.len) {
        buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE("WEBS", "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE("WEBS", "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        if (strcmp((const char*)ws_pkt.payload, "#$##$$#GETINITDATA\r\n") == 0) {  // parse here, since we shouldn't sent it to pp
            // get currently running esp app
            AppManager::sendCurrentAppToWeb();
            // lastly: send the pp connection data
            if ((PPShellComm::getAnyConnected() & 2) == 2) {
                ws_notify_cc_i2c();
            }
            free(buf);
            return ESP_OK;
        }

        if (strcmp((const char*)ws_pkt.payload, "#$##$$#GETUSBSTATE\r\n") == 0) {  // parse here, since we shouldn't sent it to pp
            if ((PPShellComm::getAnyConnected() & 2) == 2) {
                ws_notify_cc_i2c();
            }
            free(buf);
            return ESP_OK;
        }
        if (strcmp((const char*)ws_pkt.payload, "#$##$$#DISABLEESPASYNC\r\n") == 0) {  // parse here, since we shouldn't sent it to pp
            // disable async
            disable_esp_async = true;
            free(buf);
            return ESP_OK;
        }
        if (strcmp((const char*)ws_pkt.payload, "#$##$$#ENABLEESPASYNC\r\n") == 0) {  // parse here, since we shouldn't sent it to pp
            // enable async
            disable_esp_async = false;
            free(buf);
            return ESP_OK;
        }
        if (strcmp((const char*)ws_pkt.payload, "#$##$$#GPSDEBUGON\r\n") == 0) {  // parse here, since we shouldn't sent it to pp
            // enable async
            gpsDebug = true;
            free(buf);
            return ESP_OK;
        }
        if (strcmp((const char*)ws_pkt.payload, "#$##$$#GPSDEBUGOFF\r\n") == 0) {  // parse here, since we shouldn't sent it to pp
            // enable async
            gpsDebug = false;
            free(buf);
            return ESP_OK;
        }
        if (AppManager::handleWebData((const char*)ws_pkt.payload, ws_pkt.len)) {
            // handled by app
            free(buf);
            return ESP_OK;
        }
        PPShellComm::write(ws_pkt.payload, ws_pkt.len, false, true);
        free(buf);
    }
    return ESP_OK;
}

// config web server part
static httpd_handle_t setup_websocket_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
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
    httpd_uri_t uri_getpinconfig = {.uri = "/pinconfig.html",
                                    .method = HTTP_GET,
                                    .handler = get_req_handler_pinconfig,
                                    .user_ctx = NULL,
                                    .is_websocket = false,
                                    .handle_ws_control_frames = false,
                                    .supported_subprotocol = NULL};
    httpd_uri_t uri_postpinconfig = {.uri = "/pinconfig.html",
                                     .method = HTTP_POST,
                                     .handler = post_req_handler_pinconfig,
                                     .user_ctx = NULL,
                                     .is_websocket = false,
                                     .handle_ws_control_frames = false,
                                     .supported_subprotocol = NULL};
    httpd_uri_t uri_getsetupcss = {.uri = "/setup.css",
                                   .method = HTTP_GET,
                                   .handler = get_req_handler_setupcss,
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

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_getsetup);
        httpd_register_uri_handler(server, &uri_postsetup);
        httpd_register_uri_handler(server, &uri_getpinconfig);
        httpd_register_uri_handler(server, &uri_postpinconfig);
        httpd_register_uri_handler(server, &uri_getsetupcss);
        httpd_register_uri_handler(server, &uri_getota);
        httpd_register_uri_handler(server, &update_post);
        httpd_register_uri_handler(server, &ws);
    }

    return server;
}

// start web server, set it up,..
void init_httpd() {
    server = setup_websocket_server();
}
