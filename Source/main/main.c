// TODO CHECK NMEA PARSER. DATE BAD WHEN NO FIX
// todo save hmc calibration, and load and use, and recalibrate on the fly

// no need to change, just connect to the AP, and change in the settings.
#define DEFAULT_WIFI_HOSTNAME "ESP32PP"
#define DEFAULT_WIFI_AP "ESP32PP"
#define DEFAULT_WIFI_PASS "12345678"
#define DEFAULT_WIFI_STA "Hotspot"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "driver/temperature_sensor.h"

static const char *TAG = "ESP32PP";

#include "webserver.h"

#include "nmea_parser.h"
#include "usbpart.h"
#include "wifim.h"

#include "orientation.h"

bool hcmInited = false;

float heading = 0.0;
float temperatureEsp = 0.0;
float temperature = 0.0;
uint8_t humidity = 0;
gps_t gpsdata;

static void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  gps_t *gps = NULL;
  switch (event_id)
  {
  case GPS_UPDATE:
    gps = (gps_t *)event_data;
    gpsdata.altitude = gps->altitude;
    gpsdata.date = gps->date;
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
  nvs_handle_t nvs_handle;
  err = nvs_open("wifi", NVS_READWRITE, &nvs_handle); // https://github.com/espressif/esp-idf/blob/v5.1.2/examples/storage/nvs_rw_value/main/nvs_value_example_main.c
  if (err != ESP_OK)
  {
    printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    strcpy(wifiAPSSID, DEFAULT_WIFI_AP);
    strcpy(wifiAPPASS, DEFAULT_WIFI_PASS);
    strcpy(wifiStaSSID, DEFAULT_WIFI_STA);
    strcpy(wifiStaPASS, DEFAULT_WIFI_PASS);
    strcpy(wifiHostName, DEFAULT_WIFI_HOSTNAME);
  }
  else
  {
    size_t size = sizeof(wifiAPSSID);
    nvs_get_str(nvs_handle, "wifiAPSSID", wifiAPSSID, &size);
    if (strlen(wifiAPSSID) < 1)
      strcpy(wifiAPSSID, DEFAULT_WIFI_AP);
    size = sizeof(wifiAPPASS);
    nvs_get_str(nvs_handle, "wifiAPPASS", wifiAPPASS, &size);
    if (strlen(wifiAPPASS) < 1)
      strcpy(wifiAPPASS, DEFAULT_WIFI_PASS);

    size = sizeof(wifiStaSSID);
    nvs_get_str(nvs_handle, "wifiStaSSID", wifiStaSSID, &size);
    if (strlen(wifiStaSSID) < 1)
      strcpy(wifiStaSSID, DEFAULT_WIFI_STA);
    size = sizeof(wifiStaPASS);
    nvs_get_str(nvs_handle, "wifiStaPASS", wifiStaPASS, &size);
    if (strlen(wifiStaPASS) < 1)
      strcpy(wifiStaPASS, DEFAULT_WIFI_PASS);
    size = sizeof(wifiHostName);
    nvs_get_str(nvs_handle, "wifiHostName", wifiHostName, &size);
    if (strlen(wifiHostName) < 1)
    {
      strcpy(wifiHostName, DEFAULT_WIFI_HOSTNAME);
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
  }

  // end config load

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  usb_init();

  initialise_wifi();
  wifi_apsta();
  init_httpd();
  nmea_parser_config_t nmeaconfig = NMEA_PARSER_CONFIG_DEFAULT();
  nmea_parser_handle_t nmea_hdl = nmea_parser_init(&nmeaconfig);
  nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);
  esp_task_wdt_deinit();
  temperature_sensor_handle_t temp_sensor = NULL;
  temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
  ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
  ESP_LOGI(TAG, "Enable temperature sensor");
  ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
  i2cdevInit(I2C0_DEV);
  init_orientation();

  while (true)
  {
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &temperatureEsp));
    heading = get_heading_degrees();
    if (!getInCommand())
    {
      char buff[300] = {0};
      snprintf(buff, 300,
               "#$##$$#GOTGPS"
               "{\"year\":%d,\"month\":%d,\"day\":%d,\"hour\":%d,\"minute\":%d,"
               "\"sec\":%d,"
               "\"siu\":%d,"
               "\"siv\":%d,"
               "\"tempesp\":%.01f,"
               "\"temp\":%.01f,"
               "\"humi\":%d,"
               "\"head\":%.01f,"
               "\"lat\":%.06f,"
               "\"lon\":%.06f,"
               "\"alt\":%.02f,"
               "\"speed\":%f}\r\n",
               gpsdata.date.year + YEAR_BASE, gpsdata.date.month, gpsdata.date.day, gpsdata.tim.hour + TIME_ZONE, gpsdata.tim.minute, gpsdata.tim.second,
               gpsdata.sats_in_use, gpsdata.sats_in_view, temperatureEsp, temperature, humidity, heading,
               gpsdata.latitude, gpsdata.longitude, gpsdata.altitude, gpsdata.speed);
      ws_sendall((uint8_t *)buff, strlen(buff));
      char gotusb[300];
      if (gpsdata.latitude != 0 || gpsdata.longitude != 0)
      {
        snprintf(gotusb, 290, "gotgps %.06f %.06f %.02f %.01f\r\n", gpsdata.latitude, gpsdata.longitude, gpsdata.altitude, gpsdata.speed);
        ESP_LOGI(TAG, "%s", gotusb);
        wait_till_usb_sending(3000);
        write_usb_blocking((uint8_t *)gotusb, strnlen(gotusb, 290), true, false);
      }
      snprintf(gotusb, 290, "gotorientation %.01f\r\n", heading);
      ESP_LOGI(TAG, "%s", gotusb);
      wait_till_usb_sending(3000);
      write_usb_blocking((uint8_t *)gotusb, strnlen(gotusb, 290), true, false);
      ESP_LOGI(TAG, "sensorreport end");
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}