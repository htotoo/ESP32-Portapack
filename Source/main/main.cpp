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

typedef struct
{
  float latitude;       /*!< Latitude (degrees) */
  float longitude;      /*!< Longitude (degrees) */
  float altitude;       /*!< Altitude (meters) */
  uint8_t sats_in_use;  /*!< Number of satellites in use */
  uint8_t sats_in_view; /*!< Number of satellites in view */
  float speed;          /*!< Ground speed, unit: m/s */
  gps_date_t date;      /*!< Fix date */
  gps_time_t tim;       /*!< time in UTC */
} gpssmall_t;

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
gpssmall_t gpsdata;
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

/*

I2C slave device

*/

#define ESP_SLAVE_ADDR 0x51
typedef struct
{
  uint32_t api_version;
  uint32_t module_version;
  char module_name[20];
  uint32_t application_count;
} device_info;

enum app_location_t : uint32_t
{
  UTILITIES = 0,
  RX,
  TX,
  DEBUG,
  HOME
};

typedef struct
{
  uint32_t header_version;
  uint8_t app_name[16];
  uint8_t bitmap_data[32];
  uint32_t icon_color;
  app_location_t menu_location;
  uint32_t binary_size;
} standalone_app_info;

#define USER_COMMANDS_START 0x7F01

enum class Command : uint16_t
{
  COMMAND_NONE = 0,

  // will respond with device_info
  COMMAND_INFO = 0x18F0,

  // will respond with info of application
  COMMAND_APP_INFO = 0xA90B,

  // will respond with application data
  COMMAND_APP_TRANSFER = 0x4183,

  // UART specific commands
  COMMAND_UART_REQUESTDATA_SHORT = USER_COMMANDS_START,
  COMMAND_UART_REQUESTDATA_LONG,
  COMMAND_UART_BAUDRATE_INC,
  COMMAND_UART_BAUDRATE_DEC,
  COMMAND_UART_BAUDRATE_GET,
  // Sensor specific commands
  COMMAND_GETFEATURE_MASK,
  COMMAND_GETFEAT_DATA_GPS,
};

volatile Command command_state = Command::COMMAND_NONE;
volatile uint16_t app_counter = 0;
volatile uint16_t app_transfer_block = 0;

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
  if (app_counter > 0)
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
    gpsdata.date = gps->date;
    gpsdata.tim = gps->tim;
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

#include "ppi2c/i2c_slave_driver.h"

QueueHandle_t slave_queue;

void on_command_ISR(Command command, std::vector<uint8_t> additional_data)
{
  command_state = command;

  switch (command)
  {
  case Command::COMMAND_APP_INFO:
    if (additional_data.size() == 2)
      app_counter = *(uint16_t *)additional_data.data();
    break;

  case Command::COMMAND_APP_TRANSFER:
    if (additional_data.size() == 4)
    {
      app_counter = *(uint16_t *)additional_data.data();
      app_transfer_block = *(uint16_t *)(additional_data.data() + 2);
    }
    break;

  case Command::COMMAND_GETFEATURE_MASK:
    update_features();
    break;
  default:
    break;
  }

  BaseType_t high_task_wakeup = pdFALSE;
  xQueueSendFromISR(slave_queue, &command, &high_task_wakeup);
}

std::vector<uint8_t> on_send_ISR()
{
  switch (command_state)
  {
  case Command::COMMAND_INFO:
  {
    device_info info = {
        /* api_version = */ 2,
        /* module_version = */ 1,
        /* module_name = */ "ESP32PP",
        /* application_count = */ 0};

    return std::vector<uint8_t>((uint8_t *)&info, (uint8_t *)&info + sizeof(info));
  }

  case Command::COMMAND_APP_INFO:
  {
    break;
  }

  case Command::COMMAND_APP_TRANSFER:
  {
    break;
  }

  case Command::COMMAND_GETFEATURE_MASK:
  {
    uint64_t features = chipFeatures.getFeatures();
    return std::vector<uint8_t>((uint8_t *)&features, (uint8_t *)&features + sizeof(features));
  }

  case Command::COMMAND_GETFEAT_DATA_GPS:
  {
    return std::vector<uint8_t>((uint8_t *)&gpsdata, (uint8_t *)&gpsdata + sizeof(gpsdata));
  }

  default:
    break;
  }

  return {0xFF};
}

bool i2c_slave_callback_ISR(struct i2c_slave_device_t *dev, I2CSlaveCallbackReason reason)
{
  switch (reason)
  {
  case I2C_CALLBACK_REPEAT_START:
    break;

  case I2C_CALLBACK_SEND_DATA:
    if (dev->state == I2C_STATE_SEND)
    {
      auto data = on_send_ISR();
      if (data.size() > 0)
        i2c_slave_send_data(dev, data.data(), data.size());
    }
    break;

  case I2C_CALLBACK_DONE:
    if (dev->state == I2C_STATE_RECV)
    {
      uint16_t command = *(uint16_t *)&dev->buffer[dev->bufstart];
      std::vector<uint8_t> additional_data(dev->buffer + dev->bufstart + 2, dev->buffer + dev->bufend);

      on_command_ISR((Command)command, additional_data);
    }
    break;

  default:
    return false;
  }

  return true;
}

i2c_slave_config_t i2c_slave_config = {
    i2c_slave_callback_ISR,
    ESP_SLAVE_ADDR,
    (gpio_num_t)CONFIG_I2C_SLAVE_SCL_IO,
    (gpio_num_t)CONFIG_I2C_SLAVE_SDA_IO,
    I2C_NUM_1};

i2c_slave_device_t *slave_device;

void initialize_fixed_i2c()
{
  slave_queue = xQueueCreate(1, sizeof(uint16_t));

  ESP_ERROR_CHECK(i2c_slave_new(&i2c_slave_config, &slave_device));
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
    initialize_fixed_i2c();

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