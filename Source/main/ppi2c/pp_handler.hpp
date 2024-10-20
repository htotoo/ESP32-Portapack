
#include "i2c_slave_driver.h"
#include "pp_structures.hpp"
#include <iostream>
#include <memory>
#include <cstring>
#include <vector>
#include <queue>
#include <algorithm>
#include "driver/i2c.h"

class PPHandler
{
public:
    static void init(gpio_num_t scl, gpio_num_t sda, uint8_t addr_);
    static void set_get_features_CB(get_features_CB cb);
    static void set_module_version(uint32_t version);
    static void set_module_name(std::string name);
    static void set_get_gps_data_CB(get_gps_data_CB cb);
    static void set_get_orientation_data_CB(get_orientation_data_CB cb);
    static void set_get_environment_data_CB(get_environment_data_CB cb);
    static void set_get_light_data_CB(get_light_data_CB cb);
    static uint16_t get_appCount();

private:
    // base working code
    static bool i2c_slave_callback_ISR(struct i2c_slave_device_t *dev, I2CSlaveCallbackReason reason);
    static std::vector<uint8_t> on_send_ISR();
    static void on_command_ISR(Command command, std::vector<uint8_t> additional_data);
    static uint8_t addr; // my i2c address
    static i2c_slave_device_t *slave_device;
    static QueueHandle_t slave_queue;
    static uint32_t module_version;
    static char module_name[20];

    static volatile Command command_state; // current command
    static volatile uint16_t app_counter;  // for transfer
    static volatile uint16_t app_transfer_block;

    static uint16_t appCount; // all apps count i have.

    // callbacks
    static get_features_CB features_cb;
    static get_gps_data_CB gps_data_cb;
    static get_orientation_data_CB orientation_data_cb;
    static get_environment_data_CB environment_data_cb;
    static get_light_data_CB light_data_cb;
};
