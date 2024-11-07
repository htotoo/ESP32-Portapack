/*
 * Copyright (C) 2024 HTotoo
 *
 * This file is part of ESP32-Portapack.
 *
 * For additional license information, see the LICENSE file.
 */

// TODO CHECK NMEA PARSER. DATE BAD WHEN NO FIX
// todo save hmc calibration, and load and use, and recalibrate on the fly

// todo add an option to disable rtc set (bc utc only)
// todo add +- time

// rgb led: GPIO48 on ESP S3. set to -1 to disable
#define RGB_LED_PIN 48

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "configuration.h"
#include "esp_spiffs.h"
#include "esp_http_client.h"
#include "esp_sntp.h"

#include "sensordb.h"

#include <driver/temperature_sensor.h>

#include "webserver.h"

#include "nmea_parser.h"
#include "wifim.h"
#include "ppshellcomm.h"
#include "orientation.h"
#include "environment.h"

#include "ppi2c/pp_handler.hpp"

#define PPCMD_SATTRACK_DATA 0xa000
#define PPCMD_SATTRACK_SETSAT 0xa001
#define PPCMD_SATTRACK_SETMGPS 0xa002
uint8_t time_method = 0; // 0 = no valid, 1 = gps, 2 = ntp

#include "sgp4/Sgp4.h"
#include "../extapps/sattrack.h"
#include "../extapps/digitalrain.h"

#define TAG "ESP32PP"

Sgp4 sat;

typedef enum TimerEntry
{
    TimerEntry_SENSORGET,
    TimerEntry_REPORTPPGPS,
    TimerEntry_REPORTPPORI,
    TimerEntry_REPORTPPENVI,
    TimerEntry_REPORTPPTIME,
    TimerEntry_REPORTWEB,
    TimerEntry_REPORTRGB,
    TimerEntry_SATTRACK,
    TimerEntry_SATDOWN,
    TimerEntry_MAX
} TimerEntry;

uint32_t time_millis = 0; // current time in millis

uint32_t last_millis[TimerEntry_MAX] = {0};
// ______________________________________SEN    GPS  ORI    ENV   TIME        WEB   RGB   SAT    DOWN
uint32_t timer_millis[TimerEntry_MAX] = {2000, 2000, 1000, 2000, 60000 * 10, 2000, 1000, 2000, 20000};

float heading = 400.0;
float tilt = 0.0;
float temperatureEsp = 0.0;
float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;
uint16_t light = 0;
ppgpssmall_t gpsdata;
bool gotAnyGps = false;
bool downloadedTLE = false;
uint16_t lastReportedMxS = 0; // gps last reported gps time mix to see if it is changed. if not changes, it stuck (bad signal, no update), so won't update PP based on it
uint32_t gps_baud = 9600;

uint32_t i2c_pp_last_comm_time = 0; // when is it connected last time (last query from pp).
bool i2p_pp_conn_state = false;     // to save and check if i need to send a message to web

typedef struct
{
    float lat;
    float lon;
} sat_mgps_t;

typedef struct
{
    float azimuth;
    float elevation;
    uint8_t day;   // data time for setting when the data last updated, and to let user check if it is ok or not
    uint8_t month; // lat lon won't sent with this, since it is queried by driver
    uint16_t year;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t sat_day; // sat last data
    uint8_t sat_month;
    uint16_t sat_year;
    uint8_t sat_hour;
    float lat;
    float lon;
    uint8_t time_method;
} sattrackdata_t;
sattrackdata_t sattrackdata;
uint8_t sattrack_task = 0; // this will set to the main thread what is the current task. 0 = none, 1 = update db, 2 = change sat. each task needs to reset it.
uint8_t sattrack_task_last_error = 0;
std::string sat_to_track = "NOAA 15";
std::string sat_to_track_new = "";
bool sat_data_loaded = false;

#include "led.h"

static void i2c_scan()
{
    vTaskDelay(50 / portTICK_PERIOD_MS); // wait till devs wake up
    esp_err_t res;
    printf("i2c scan: \n");
    i2c_dev_t dev = {};
    dev.cfg.sda_io_num = CONFIG_IC2SDAPIN;
    dev.cfg.scl_io_num = CONFIG_IC2SCLPIN;
    dev.cfg.master.clk_speed = 400000;
    for (uint8_t i = 1; i < 127; i++)
    {
        dev.addr = i;
        res = i2c_dev_probe(&dev, I2C_DEV_WRITE);
        if (res == 0)
        {
            printf("Found device at: 0x%2x\n", i);
            foundI2CDev(i);
        }
    }
}

ChipFeatures chipFeatures{};
void update_features()
{
    chipFeatures.reset();
    if (is_environment_light_sensor_present())
        chipFeatures.enableFeature(SupportedFeatures::FEAT_LIGHT);
    if (is_environment_sensor_present())
        chipFeatures.enableFeature(SupportedFeatures::FEAT_ENVIRONMENT);
    if (is_orientation_sensor_present())
        chipFeatures.enableFeature(SupportedFeatures::FEAT_ORIENTATION);
    if (gotAnyGps)
        chipFeatures.enableFeature(SupportedFeatures::FEAT_GPS);
    if (PPHandler::get_appCount() > 0)
        chipFeatures.enableFeature(SupportedFeatures::FEAT_EXT_APP);
    // uart is not present
    // display is not present
    chipFeatures.enableFeature(SupportedFeatures::FEAT_SHELL);
}

esp_err_t init_spiffs(void)
{
    // Configuration settings for SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",        // Mount point for the file system
        .partition_label = NULL,       // Use default partition
        .max_files = 5,                // Maximum number of open files
        .format_if_mount_failed = true // Don't format on mount failure automatically
    };

    // Initialize SPIFFS with the configuration
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    // Check if SPIFFS was mounted successfully
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SPIFFS mounted successfully.");
    }
    else if (ret == ESP_FAIL)
    {
        ESP_LOGE(TAG, "Failed to mount SPIFFS, attempting to format...");

        // Try formatting SPIFFS
        ret = esp_spiffs_format(NULL); // Pass partition label if needed
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "SPIFFS formatted successfully, retrying mount...");

            // Retry mounting after format
            ret = esp_vfs_spiffs_register(&conf);
            if (ret == ESP_OK)
            {
                ESP_LOGI(TAG, "SPIFFS mounted successfully after format.");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to mount SPIFFS after formatting.");
            }
        }
        else
        {
            ESP_LOGE(TAG, "SPIFFS format failed.");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }

    // Check the SPIFFS partition size and used space
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SPIFFS: total: %d bytes, used: %d bytes", total, used);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief HTTP event handler to handle HTTP client events during the download.
 *
 * @param evt HTTP client event.
 * @return esp_err_t Returns ESP_OK to continue the operation, or an error code to abort.
 */
esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    static FILE *file = NULL;

    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        if (!file)
        {
            // Open the file for writing when the first data chunk is received
            file = fopen("/spiffs/mini.tle", "w");
            if (file == NULL)
            {
                ESP_LOGE(TAG, "Failed to open file for writing");
                return ESP_FAIL;
            }
        }

        // Write the data chunk to the file
        if (fwrite(evt->data, 1, evt->data_len, file) != evt->data_len)
        {
            ESP_LOGE(TAG, "File write failed!");
            fclose(file);
            file = NULL;
            return ESP_FAIL;
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        if (file)
        {
            fclose(file);
            file = NULL;
            ESP_LOGI(TAG, "File download completed");
        }
        break;

    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP event error");
        if (file)
        {
            fclose(file);
            file = NULL;
        }
        break;

    default:
        break;
    }
    return ESP_OK;
}

esp_err_t load_satellite_tle(const std::string &sat_to_track)
{
    std::string l1, l2;
    // Open the TLE file from SPIFFS
    std::ifstream file("/spiffs/mini.tle");
    if (!file.is_open())
    {
        ESP_LOGE(TAG, "Failed to open file: /spiffs/mini.tle");
        return ESP_FAIL;
    }

    std::string line;
    bool satellite_found = false;

    // Read the file line by line
    while (std::getline(file, line))
    {
        // Check if the current line starts with the satellite name
        if (line.find(sat_to_track) == 0)
        {
            ESP_LOGI(TAG, "Found satellite: %s", sat_to_track.c_str());

            // Read the next two lines (the TLE data)
            if (std::getline(file, l1) && std::getline(file, l2))
            {
                ESP_LOGI(TAG, "TLE Line 1: %s", l1.c_str());
                ESP_LOGI(TAG, "TLE Line 2: %s", l2.c_str());
                sat.init(sat_to_track.c_str(), (char *)l1.c_str(), (char *)l2.c_str());
                double satjs = sat.satrec.jdsatepoch;
                int y, m, d, h, mi;
                double s;
                invjday(satjs, 0, false, y, m, d, h, mi, s);
                sattrackdata.sat_day = d;
                sattrackdata.sat_month = m;
                sattrackdata.sat_year = y;
                sattrackdata.sat_hour = h;
                satellite_found = true;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to read TLE lines after satellite name.");
                file.close();
                return ESP_FAIL;
            }
            break;
        }
    }

    // Close the file
    file.close();

    // Return status based on whether the satellite was found
    if (satellite_found)
    {
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Satellite %s not found in TLE file.", sat_to_track.c_str());
        return ESP_FAIL;
    }
}

/**
 * @brief Downloads the file from the given URL and saves it to SPIFFS.
 *
 * @return esp_err_t Returns ESP_OK if the download and save were successful, or an error code otherwise.
 */
esp_err_t download_file_to_spiffs(void)
{
    esp_err_t ret;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    // Configure the HTTP client
    esp_http_client_config_t config = {
        .url = "http://creativo.hu/sattrack/mini.tle",
        .event_handler = http_event_handler,
    };
#pragma GCC diagnostic pop

    ESP_LOGI(TAG, "HTTP GET request started");
    // Initialize the HTTP client
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Perform the HTTP GET request to download the file
    ret = esp_http_client_perform(client);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET request completed");
        downloadedTLE = true;
        sat_to_track_new = sat_to_track;
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(ret));
    }

    // Clean up
    esp_http_client_cleanup(client);

    return ret;
}

static void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    gps_t *gps = NULL;
    switch (event_id)
    {
    case GPS_UPDATE:
        gotAnyGps = true;
        gps = (gps_t *)event_data;
        gpsdata.altitude = gps->altitude;
        gpsdata.date.day = gps->date.day;
        gpsdata.date.month = gps->date.month;
        gpsdata.date.year = gps->date.year;
        gpsdata.tim.hour = gps->tim.hour;
        gpsdata.tim.minute = gps->tim.minute;
        gpsdata.tim.second = gps->tim.second;
        gpsdata.latitude = gps->latitude;
        gpsdata.longitude = gps->longitude;
        gpsdata.speed = gps->speed;
        gpsdata.sats_in_use = gps->sats_in_use;
        gpsdata.sats_in_view = gps->sats_in_view;

        break;
    case GPS_UNKNOWN:
        // ESP_LOGW(TAG, "Unknown statement:%s", (char*)event_data);
        break;
    default:
        break;
    }
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized from SNTP server");
    if (time_method == 0)
        time_method = 2;
}

#if __cplusplus
extern "C"
{
#endif
    void app_main(void)
    {
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }
        ESP_ERROR_CHECK(err);
        // load prev settings
        WifiM::load_config_wifi();
        load_config_misc();

        init_spiffs();
        sat.site(0, 0, 34);
        sat_data_loaded = (load_satellite_tle(sat_to_track) == ESP_OK);

        // end config load
        i2cdev_init();
        // i2c scanner
        i2c_scan();

        ESP_ERROR_CHECK(esp_event_loop_create_default());
        PPShellComm::init();

        WifiM::initialise_wifi();
        WifiM::wifi_apsta();
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
        sntp_set_time_sync_notification_cb(time_sync_notification_cb);

        init_httpd();
        nmea_parser_config_t nmeaconfig = NMEA_PARSER_CONFIG_DEFAULT();
        nmeaconfig.uart.baud_rate = gps_baud;
        nmea_parser_handle_t nmea_hdl = nmea_parser_init(&nmeaconfig);
        nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);
        esp_task_wdt_deinit();

        temperature_sensor_handle_t temp_sensor = NULL;
        temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
        ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
        ESP_LOGI(TAG, "Enable temperature sensor");
        ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));

        LedFeedback::init(RGB_LED_PIN);
        LedFeedback::rgb_set(255, 255, 255);

        init_orientation(); // it loads orientation data too
        init_environment();

        i2c_scan();

        PPHandler::set_module_name("ESP32PP");
        PPHandler::set_module_version(1);
        PPHandler::init((gpio_num_t)CONFIG_I2C_SLAVE_SCL_IO, (gpio_num_t)CONFIG_I2C_SLAVE_SDA_IO, 0x51);
        PPHandler::add_app((uint8_t *)sattrack, sizeof(sattrack));
        PPHandler::add_app((uint8_t *)digitalrain, sizeof(digitalrain));
        PPHandler::set_get_features_CB([](uint64_t &feat)
                                       {
                                        i2c_pp_last_comm_time = time_millis;
                                    update_features();
                                    feat = chipFeatures.getFeatures(); });

        PPHandler::set_get_gps_data_CB([](ppgpssmall_t &gpsdata)
                                       { gpsdata = gpsdata; i2c_pp_last_comm_time = time_millis; });

        PPHandler::set_get_orientation_data_CB([](orientation_t &ori)
                                               {
                                                ori.angle = heading;
                                                ori.tilt = tilt; i2c_pp_last_comm_time = time_millis; });
        PPHandler::set_get_environment_data_CB([](environment_t &env)
                                               { 
                                                i2c_pp_last_comm_time = time_millis;
                                                env.temperature = temperature;
                                                env.humidity = humidity;
                                                env.pressure = pressure; });
        PPHandler::set_get_light_data_CB([](uint16_t &light)
                                         { light = light; });

        PPHandler::add_custom_command(PPCMD_SATTRACK_DATA, nullptr, [](pp_command_data_t data)
                                      {
                                        data.data->resize(sizeof(sattrackdata_t));
                                        *(sattrackdata_t *)(*data.data).data() = sattrackdata; });

        PPHandler::add_custom_command(PPCMD_SATTRACK_SETMGPS, [](pp_command_data_t data)
                                      {
                                        if (data.data->size() != sizeof(sat_mgps_t)) {
                                            return;
                                        }
                                        sat_mgps_t tmp;
                                        memcpy(&tmp, data.data->data(), sizeof(sat_mgps_t));
                                        sattrackdata.elevation = 0;
                                        sattrackdata.azimuth = 0;
                                        sattrackdata.lat = tmp.lat;
                                        sattrackdata.lon = tmp.lon; }, nullptr);

        PPHandler::add_custom_command(PPCMD_SATTRACK_SETSAT, [](pp_command_data_t data)
                                      {
                                        std::string str;
                                        for(int i = 0; i < data.data->size(); i++)
                                        {
                                            if (data.data->at(i) == 0) break;
                                            str += (char)data.data->at(i);
                                        }
                                        sat_to_track_new =str; }, nullptr);

        PPHandler::set_get_shell_data_size_CB([]() -> uint16_t
                                              {i2c_pp_last_comm_time = time_millis; return PPShellComm::get_i2c_tx_queue_size(); });

        PPHandler::set_got_shell_data_CB([](std::vector<uint8_t> &data)
                                         { I2CQueueMessage_t msg;
                                           i2c_pp_last_comm_time = time_millis;
                                           msg.size = data.size();
                                           size_t size = data.size();
                                           if (size > 64)
                                           {
                                             size = 64;
                                           }
                                           memcpy(msg.data, data.data(), size);
                                           auto ttt = pdFALSE;
                                           xQueueSendFromISR(PPShellComm::datain_queue, &msg, &ttt); });

        PPHandler::set_send_shell_data_CB([](std::vector<uint8_t> &data, bool &hasmore)
                                          {
                                            hasmore = false;
                                            data.resize(64); 
                                            size_t size = PPShellComm::get_i2c_tx_queue_size();
                                            if (size>64)
                                            {
                                                hasmore = true;
                                                size = 64;
                                            }
                                            memcpy(data.data(), PPShellComm::get_i2c_tx_queue_data(), size);
                                            PPShellComm::clear_tx_queue(); });

        // shell helper
        PPShellComm::set_data_rx_callback([](const uint8_t *data, size_t data_len) -> bool
                                          {
                                                    ws_sendall((uint8_t*)data, data_len);
                                                    return true; });
        while (true)
        {
            time_millis = esp_timer_get_time() / 1000;
            if (sat_to_track_new != "")
            {
                sat_to_track = sat_to_track_new;
                sat_data_loaded = (load_satellite_tle(sat_to_track) == ESP_OK);
                sattrackdata.elevation = 0;
                sattrackdata.azimuth = 0;
                last_millis[TimerEntry_SATTRACK] = 10; // force update
                sat_to_track_new = "";
            }
            // GET ALL SENSOR DATA
            if (time_millis - last_millis[TimerEntry_SENSORGET] > timer_millis[TimerEntry_SENSORGET])
            {
                // GPS IS AUTO
                ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &temperatureEsp)); // TEMPINT
                heading = get_heading_degrees();                                               // ORIENTATION
                tilt = get_tilt();                                                             // TILT
                get_environment_meas(&temperature, &pressure, &humidity);                      // env data
                get_environment_light(&light);                                                 // light (lux) data
                last_millis[TimerEntry_SENSORGET] = time_millis;
            }

            // REPORT ALL SENSOR DATA TO WEB
            if (!PPShellComm::getInCommand() && (time_millis - last_millis[TimerEntry_REPORTWEB] > timer_millis[TimerEntry_REPORTWEB]))
            {
                char buff[300] = {0};
                snprintf(buff, 300,
                         "#$##$$#GOTSENS"
                         "{\"gps\":{\"y\":%d,\"m\":%d,\"d\":%d,\"h\":%d,\"mi\":%d,\"s\":%d,"
                         "\"siu\":%d,\"siv\":%d,\"lat\":%.06f,\"lon\":%.06f,\"alt\":%.02f,\"speed\":%f},"
                         "\"ori\":{\"head\":%.01f, \"tilt\":%.01f },"
                         "\"env\":{\"tempesp\":%.01f,\"temp\":%.01f,\"humi\":%.01f, \"press\":%.01f, \"light\":%d }"
                         "}\r\n",
                         gpsdata.date.year + YEAR_BASE, gpsdata.date.month, gpsdata.date.day, gpsdata.tim.hour + TIME_ZONE, gpsdata.tim.minute, gpsdata.tim.second,
                         gpsdata.sats_in_use, gpsdata.sats_in_view, gpsdata.latitude, gpsdata.longitude, gpsdata.altitude, gpsdata.speed,
                         heading, tilt,
                         temperatureEsp, temperature, humidity, pressure, light);
                ws_sendall((uint8_t *)buff, strlen(buff));
                last_millis[TimerEntry_REPORTWEB] = time_millis;
            }

            // REPORT SENSOR DATA TO PP. EACH HAS OWN TIMER!
            char gotusb[300];
            if (PPShellComm::getAnyConnected() == 1 && !PPShellComm::getInCommand() && (time_millis - last_millis[TimerEntry_REPORTPPGPS] > timer_millis[TimerEntry_REPORTPPGPS]))
            {
                if (gpsdata.latitude != 0 || gpsdata.longitude != 0)
                {
                    snprintf(gotusb, 290, "gotgps %.06f %.06f %.02f %.01f %d\r\n", gpsdata.latitude, gpsdata.longitude, gpsdata.altitude, gpsdata.speed, gpsdata.sats_in_use);
                    ESP_LOGI(TAG, "%s", gotusb);
                    if (PPShellComm::wait_till_sending(1))
                    {
                        PPShellComm::write_blocking((uint8_t *)gotusb, strnlen(gotusb, 290), true, false);
                        ESP_LOGI(TAG, "gotgps sent");
                    }
                    last_millis[TimerEntry_REPORTPPGPS] = time_millis;
                }
            }
            if (PPShellComm::getAnyConnected() == 1 && !PPShellComm::getInCommand() && (time_millis - last_millis[TimerEntry_REPORTPPORI] > timer_millis[TimerEntry_REPORTPPORI]))
            {
                if (heading < 400) // got heading data
                {
                    snprintf(gotusb, 290, "gotorientation %.01f %.01f\r\n", heading, tilt);
                    ESP_LOGI(TAG, "%s", gotusb);
                    if (PPShellComm::wait_till_sending(1))
                    {
                        PPShellComm::write_blocking((uint8_t *)gotusb, strnlen(gotusb, 290), true, false);
                        ESP_LOGI(TAG, "gotorientation sent");
                    }
                    last_millis[TimerEntry_REPORTPPORI] = time_millis;
                }
            }
            if (PPShellComm::getAnyConnected() == 1 && !PPShellComm::getInCommand() && (time_millis - last_millis[TimerEntry_REPORTPPENVI] > timer_millis[TimerEntry_REPORTPPENVI]))
            {
                if (heading < 400) // got heading data
                {
                    snprintf(gotusb, 290, "gotenv %.02f %.01f %.02f %d\r\n", temperature, humidity, pressure, light);
                    ESP_LOGI(TAG, "%s", gotusb);
                    if (PPShellComm::wait_till_sending(1))
                    {
                        PPShellComm::write_blocking((uint8_t *)gotusb, strnlen(gotusb, 290), true, false);
                        ESP_LOGI(TAG, "gotenv sent");
                    }
                    last_millis[TimerEntry_REPORTPPENVI] = time_millis;
                }
            }
            if (PPShellComm::getAnyConnected() == 1 && !PPShellComm::getInCommand() && (time_millis - last_millis[TimerEntry_REPORTPPTIME] > timer_millis[TimerEntry_REPORTPPTIME]))
            {
                uint16_t cmxs = gpsdata.tim.minute * gpsdata.tim.second + gpsdata.tim.second + gpsdata.tim.hour;
                if (gpsdata.date.year < 44 && gpsdata.date.year >= 23 && lastReportedMxS != cmxs) // got a valid time, and ti is not the last
                {
                    snprintf(gotusb, 290, "rtcset %d %d %d %d %d %d\r\n", gpsdata.date.year, gpsdata.date.month, gpsdata.date.day, gpsdata.tim.hour, gpsdata.tim.minute, gpsdata.tim.second);
                    ESP_LOGI(TAG, "%s", gotusb);
                    if (PPShellComm::wait_till_sending(1))
                    {
                        PPShellComm::write_blocking((uint8_t *)gotusb, strnlen(gotusb, 290), true, false);
                        ESP_LOGI(TAG, "settime sent");
                        lastReportedMxS = cmxs;
                    }
                    last_millis[TimerEntry_REPORTPPTIME] = time_millis;
                }
            }

            if (time_millis - last_millis[TimerEntry_REPORTRGB] > timer_millis[TimerEntry_REPORTRGB])
            {
                LedFeedback::rgb_set_by_status(PPShellComm::getAnyConnected() | i2p_pp_conn_state, WifiM::getWifiStaStatus(), WifiM::getWifiApClientNum() > 0, gpsdata.latitude != 0 && gpsdata.longitude != 0 && gpsdata.sats_in_use > 2);
                last_millis[TimerEntry_REPORTRGB] = time_millis;
            }

            if (time_millis - last_millis[TimerEntry_SATTRACK] > timer_millis[TimerEntry_SATTRACK])
            {
                // check for new gps data
                // ESP_LOGI(TAG, "qgps: %f  %f", sattrackdata.lat, sattrackdata.lon);
                if (sat_data_loaded)
                {
                    if (gpsdata.latitude != 0 || gpsdata.longitude != 0)
                    {
                        sattrackdata.lat = gpsdata.latitude;
                        sattrackdata.lon = gpsdata.longitude;
                    }
                    // if only old, or etc use that nvm
                    if (sattrackdata.lat != 0 || sattrackdata.lon != 0)
                    {
                        struct tm timeinfo;
                        sat.site(gpsdata.latitude, gpsdata.longitude, gpsdata.altitude);
                        double jd = 0;
                        if (gpsdata.date.year < 44 && gpsdata.date.year >= 23) // has valid gps time
                        {
                            time_method = 1;
                            jday(gpsdata.date.year + YEAR_BASE, gpsdata.date.month, gpsdata.date.day, gpsdata.tim.hour, gpsdata.tim.minute, gpsdata.tim.second, 0, false, jd);
                        }
                        else
                        {
                            time_t now;
                            time(&now);
                            localtime_r(&now, &timeinfo);
                            int year = timeinfo.tm_year + 1900; // Az év 1900-tól számítva
                            int month = timeinfo.tm_mon + 1;    // A hónap 0-11, ezért +1
                            int day = timeinfo.tm_mday;

                            int hour = timeinfo.tm_hour;
                            int minute = timeinfo.tm_min;
                            int second = timeinfo.tm_sec;
                            jday(year, month, day, hour, minute, second, 0, false, jd);
                        }
                        sat.findsat(jd);
                        if (time_method == 0)
                        {
                            sattrackdata.azimuth = 0;
                            sattrackdata.elevation = 0;
                        }
                        else
                        {
                            sattrackdata.azimuth = sat.satAz;
                            sattrackdata.elevation = sat.satEl;
                        }
                        if (time_method == 1)
                        {
                            sattrackdata.day = gpsdata.date.day;
                            sattrackdata.month = gpsdata.date.month;
                            sattrackdata.year = gpsdata.date.year;
                            sattrackdata.hour = gpsdata.tim.hour;
                            sattrackdata.minute = gpsdata.tim.minute;
                            sattrackdata.second = gpsdata.tim.second;
                        }
                        else
                        {
                            sattrackdata.day = timeinfo.tm_mday;
                            sattrackdata.month = timeinfo.tm_mon + 1;
                            sattrackdata.year = timeinfo.tm_year + 1900;
                            sattrackdata.hour = timeinfo.tm_hour;
                            sattrackdata.minute = timeinfo.tm_min;
                            sattrackdata.second = timeinfo.tm_sec;
                        }
                        sattrackdata.time_method = time_method;
                    }
                    last_millis[TimerEntry_SATTRACK] = time_millis;
                }
                else
                {
                    sattrackdata.sat_day = 0;
                    sattrackdata.sat_month = 0;
                    sattrackdata.sat_year = 0;
                    sattrackdata.sat_hour = 0;
                    sattrackdata.azimuth = 0;
                    sattrackdata.elevation = 0;
                    last_millis[TimerEntry_SATTRACK] = time_millis;
                }
            }

            if (time_millis - last_millis[TimerEntry_SATDOWN] > timer_millis[TimerEntry_SATDOWN])
            {
                if (!downloadedTLE)
                    download_file_to_spiffs();
                last_millis[TimerEntry_SATDOWN] = time_millis;
            }

            // check if i2c connected or dc
            if (i2c_pp_last_comm_time != 0 && (time_millis - i2c_pp_last_comm_time > 21000)) // pp query time 5 sec, so 20 is a good timeout
            {
                i2c_pp_last_comm_time = 0; // reset time
                PPShellComm::set_i2c_connected(false);
                ws_notify_dc_i2c(); // send to webpage it is dc
                i2p_pp_conn_state = false;
            }
            else if (i2c_pp_last_comm_time != 0) // no dc, but communication in last 20 sec
            {
                if (!i2p_pp_conn_state) // if it was disconnecdet before, and now it is connected, notify
                {
                    PPShellComm::set_i2c_connected(true);
                    ws_notify_cc_i2c();
                    i2p_pp_conn_state = true;
                }
            }

            // try wifi client connect
            WifiM::wifi_loop(time_millis);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }

#if __cplusplus
}
#endif