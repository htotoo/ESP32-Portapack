// TODO CHECK NMEA PARSER. DATE BAD WHEN NO FIX
// todo save hmc calibration, and load and use, and recalibrate on the fly

// todo probe multiple i2c addr per device?
// todo set brightness from web setup
// todo add an option to disable rtc set (bc utc only) //todo add +- time

// rgb led: GPIO48 on ESP S3. set to -1 to disable
#define RGB_LED_PIN 48

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
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

#include "sensordb.h"

#include <driver/temperature_sensor.h>

static const char *TAG = "ESP32PP";

#include "webserver.h"

#include "nmea_parser.h"
#include "usbpart.h"
#include "wifim.h"

#include "orientation.h"
#include "environment.h"

#include "ppi2c/pp_handler.hpp"

typedef enum TimerEntry
{
  TimerEntry_SENSORGET,
  TimerEntry_REPORTPPGPS,
  TimerEntry_REPORTPPORI,
  TimerEntry_REPORTPPENVI,
  TimerEntry_REPORTPPTIME,
  TimerEntry_REPORTWEB,
  TimerEntry_REPORTRGB,
  TimerEntry_MAX
} TimerEntry;

uint32_t last_millis[TimerEntry_MAX] = {0};
//                                       SEN    GPS  ORI    ENV   TIME        WEB   RGB
uint32_t timer_millis[TimerEntry_MAX] = {2000, 2000, 1000, 2000, 60000 * 10, 2000, 1000};

float heading = 0.0;
float tilt = 0.0;
float temperatureEsp = 0.0;
float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;
uint16_t light = 0;
ppgpssmall_t gpsdata;
bool gotAnyGps = false;
uint16_t lastReportedMxS = 0; // gps last reported gps time mix to see if it is changed. if not changes, it stuck (bad signal, no update), so won't update PP based on it
uint32_t gps_baud = 9600;

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

#include "features.hpp"
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
    load_config_wifi();
    load_config_misc();

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
    nmeaconfig.uart.baud_rate = gps_baud; // todo modify by config
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

    init_orientation(); // it loads orientation data too
    init_environment();

    i2c_scan();
    PPHandler::init((gpio_num_t)CONFIG_I2C_SLAVE_SCL_IO, (gpio_num_t)CONFIG_I2C_SLAVE_SDA_IO, 0x51);

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
        tilt = get_tilt();                                                             // TILT
        get_environment_meas(&temperature, &pressure, &humidity);                      // env data
        get_environment_light(&light);                                                 // light (lux) data
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
      if (getUsbConnected() && !getInCommand() && (time_millis - last_millis[TimerEntry_REPORTPPGPS] > timer_millis[TimerEntry_REPORTPPGPS]))
      {
        if (gpsdata.latitude != 0 || gpsdata.longitude != 0)
        {
          snprintf(gotusb, 290, "gotgps %.06f %.06f %.02f %.01f %d\r\n", gpsdata.latitude, gpsdata.longitude, gpsdata.altitude, gpsdata.speed, gpsdata.sats_in_use);
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
      if (getUsbConnected() && !getInCommand() && (time_millis - last_millis[TimerEntry_REPORTPPENVI] > timer_millis[TimerEntry_REPORTPPENVI]))
      {
        if (heading < 400) // got heading data
        {
          snprintf(gotusb, 290, "gotenv %.02f %.01f %.02f %d\r\n", temperature, humidity, pressure, light);
          ESP_LOGI(TAG, "%s", gotusb);
          if (wait_till_usb_sending(1))
          {
            write_usb_blocking((uint8_t *)gotusb, strnlen(gotusb, 290), true, false);
            last_millis[TimerEntry_REPORTPPENVI] = time_millis;
            ESP_LOGI(TAG, "gotenv sent");
          }
        }
      }
      if (getUsbConnected() && !getInCommand() && (time_millis - last_millis[TimerEntry_REPORTPPTIME] > timer_millis[TimerEntry_REPORTPPTIME]))
      {
        uint16_t cmxs = gpsdata.tim.minute * gpsdata.tim.second + gpsdata.tim.second + gpsdata.tim.hour;
        if (gpsdata.date.year < 2044 && gpsdata.date.year > 2023 && lastReportedMxS != cmxs) // got a valid time, and ti is not the last
        {
          snprintf(gotusb, 290, "rtcset %d %d %d %d %d %d\r\n", gpsdata.date.year, gpsdata.date.month, gpsdata.date.day, gpsdata.tim.hour, gpsdata.tim.minute, gpsdata.tim.second);
          ESP_LOGI(TAG, "%s", gotusb);
          if (wait_till_usb_sending(1))
          {
            write_usb_blocking((uint8_t *)gotusb, strnlen(gotusb, 290), true, false);
            last_millis[TimerEntry_REPORTPPTIME] = time_millis;
            ESP_LOGI(TAG, "settime sent");
            lastReportedMxS = cmxs;
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

#if __cplusplus
}
#endif