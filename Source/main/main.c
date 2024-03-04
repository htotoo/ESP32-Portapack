// TODO CHECK NMEA PARSER. DATE BAD WHEN NO FIX
// todo save hmc calibration, and load and use, and recalibrate on the fly
// SENSORS:
// - BH1750  - 0x23
// - HMC5883L  - 0x1E  - PARTLY
// - ADXL345 - 0x53  //https://github.com/craigpeacock/ESP32_Node/blob/master/main/adxl345.h
// - MPU925X ( 0x68 ) + 280  ( 0x76 )

// rgb led: GPIO48 on ESP S3. set to -1 to disable //todo set it up from web
#define RGB_LED_PIN 48

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
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <driver/temperature_sensor.h>

static const char *TAG = "ESP32PP";

#include "webserver.h"

#include "nmea_parser.h"
#include "usbpart.h"
#include "wifim.h"

#include "orientation.h"
#include "environment.h"

typedef enum TimerEntry
{
  TimerEntry_SENSORGET,
  TimerEntry_REPORTPPGPS,
  TimerEntry_REPORTPPORI,
  TimerEntry_REPORTWEB,
  TimerEntry_REPORTRGB,
  TimerEntry_MAX
} TimerEntry;

uint32_t last_millis[TimerEntry_MAX] = {0};
uint32_t timer_millis[TimerEntry_MAX] = {2000, 2000, 2000, 2000, 1000};

float heading = 0.0;
float tilt = 0.0;
float temperatureEsp = 0.0;
float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;
gps_t gpsdata;

#include "led.h"

static void i2c_scan()
{
  esp_err_t res;
  printf("i2c scan: \n");
  i2c_dev_t dev = {0};
  dev.cfg.sda_io_num = 5;
  dev.cfg.scl_io_num = 4;
  dev.cfg.master.clk_speed = 400000;
  for (uint8_t i = 1; i < 127; i++)
  {
    dev.addr = i;
    res = i2c_dev_probe(&dev, I2C_DEV_WRITE);
    if (res == 0)
    {
      printf("Found device at: 0x%2x\n", i);
    }
  }
}

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
  i2cdev_init();
  //  i2c scanner
  i2c_scan();

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

  init_rgb();
  rgb_set(255, 255, 255);

  init_orientation();
  init_environment();

  uint32_t time_millis = 0;

  while (true)
  {
    time_millis = esp_timer_get_time() / 1000;

    // GET ALL SENSOR DATA
    if (time_millis - last_millis[TimerEntry_SENSORGET] > timer_millis[TimerEntry_SENSORGET])
    {
      // GPS IS AUTO
      ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &temperatureEsp)); // TEMPINT
      heading = get_heading_degrees();                                               // ORIENTATION
      tilt = 400;                                                                    // TILT //TODO
      get_environment_meas(&temperature, &pressure, &humidity);                      // env data

      last_millis[TimerEntry_SENSORGET] = time_millis;
    }

    // REPORT ALL SENSOR DATA TO WEB
    if (!getInCommand() && (time_millis - last_millis[TimerEntry_REPORTWEB] > timer_millis[TimerEntry_REPORTWEB]))
    {
      char buff[300] = {0};
      snprintf(buff, 300,
               "#$##$$#GOTSENS"
               "{\"gps\":{\"y\":%d,\"m\":%d,\"d\":%d,\"h\":%d,\"mi\":%d,\"s\":%d,"
               "\"siu\":%d,\"siv\":%d,\"lat\":%.06f,\"lon\":%.06f,\"alt\":%.02f,\"speed\":%f},"
               "\"ori\":{\"head\":%.01f, \"tilt\":%.01f },"
               "\"env\":{\"tempesp\":%.01f,\"temp\":%.01f,\"humi\":%.01f, \"press\":%.01f }"
               "}\r\n",
               gpsdata.date.year + YEAR_BASE, gpsdata.date.month, gpsdata.date.day, gpsdata.tim.hour + TIME_ZONE, gpsdata.tim.minute, gpsdata.tim.second,
               gpsdata.sats_in_use, gpsdata.sats_in_view, gpsdata.latitude, gpsdata.longitude, gpsdata.altitude, gpsdata.speed,
               heading, tilt,
               temperatureEsp, temperature, humidity, pressure);
      ws_sendall((uint8_t *)buff, strlen(buff));
      last_millis[TimerEntry_REPORTWEB] = time_millis;
    }

    // REPORT SENSOR DATA TO PP. EACH HAS OWN TIMER!
    char gotusb[300];
    if (getUsbConnected() && !getInCommand() && (time_millis - last_millis[TimerEntry_REPORTPPGPS] > timer_millis[TimerEntry_REPORTPPGPS]))
    {
      if (gpsdata.latitude != 0 || gpsdata.longitude != 0)
      {
        snprintf(gotusb, 290, "gotgps %.06f %.06f %.02f %.01f\r\n", gpsdata.latitude, gpsdata.longitude, gpsdata.altitude, gpsdata.speed);
        ESP_LOGI(TAG, "%s", gotusb);
        if (wait_till_usb_sending(1))
        {
          write_usb_blocking((uint8_t *)gotusb, strnlen(gotusb, 290), true, false);
          last_millis[TimerEntry_REPORTPPGPS] = time_millis;
          ESP_LOGI(TAG, "gotgps sent");
        }
      }
    }
    if (getUsbConnected() && !getInCommand() && (time_millis - last_millis[TimerEntry_REPORTPPORI] > timer_millis[TimerEntry_REPORTPPORI]))
    {
      if (heading < 400) // got heading data
      {
        snprintf(gotusb, 290, "gotorientation %.01f %.01f\r\n", heading, tilt);
        ESP_LOGI(TAG, "%s", gotusb);
        if (wait_till_usb_sending(1))
        {
          write_usb_blocking((uint8_t *)gotusb, strnlen(gotusb, 290), true, false);
          last_millis[TimerEntry_REPORTPPORI] = time_millis;
          ESP_LOGI(TAG, "gotorientation sent");
        }
      }
    }
    if (time_millis - last_millis[TimerEntry_REPORTRGB] > timer_millis[TimerEntry_REPORTRGB])
    {
      rgb_set_by_status();
      last_millis[TimerEntry_REPORTRGB] = time_millis;
    }
    // try wifi client connect
    wifi_loop(time_millis);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}
