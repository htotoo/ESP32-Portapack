
#include "i2c_slave_driver.h"
#include "pp_structures.hpp"
#include <iostream>
#include <memory>
#include <cstring>
#include <vector>
#include <queue>
#include <algorithm>
#include "driver/i2c.h"

/*
    All callbasck are from IRQ, so a lot of things won't work from it. Also the code needs to be as fast as possible.
*/
class PPHandler {
   public:
    static void init(gpio_num_t scl, gpio_num_t sda, uint8_t addr_);
    static void set_module_name(std::string name);     // this will set the module name
    static void set_module_version(uint32_t version);  // this will set the module version

    static void set_get_features_CB(get_features_CB cb);                  // IRQ CALLBACK! this will be called to get the features of the module see SupportedFeatures
    static void set_get_gps_data_CB(get_gps_data_CB cb);                  // IRQ CALLBACK!  this will be called when the module asked for gps data
    static void set_get_orientation_data_CB(get_orientation_data_CB cb);  // IRQ CALLBACK!  this will be called when the module asked for orientation data
    static void set_get_environment_data_CB(get_environment_data_CB cb);  // IRQ CALLBACK!  this will be called when the module asked for environment data
    static void set_get_light_data_CB(get_light_data_CB cb);              // IRQ CALLBACK!  this will be called when the module asked for light data
    static void set_get_shell_data_size_CB(get_shell_data_size_CB cb);    // IRQ CALLBACK!  this will be called when the module asked for shell tx data size
    static void set_got_shell_data_CB(got_shell_data_CB cb);              // IRQ CALLBACK!  this will be called when the PP sent data to the shell
    static void set_send_shell_data_CB(send_shell_data_CB cb);            // IRQ CALLBACK!  this will be called when the module needs to send data to the shell (when prev get_shell_data_size_CB give >0 value)

    static uint32_t get_appCount();                       // this will return the app count
    static bool add_app(uint8_t* binary, uint32_t size);  // this will add an app to the module.app size must be %32 == 0

    static void add_custom_command(uint16_t command, pp_i2c_command got_command, pp_i2c_command send_command);  // Callbacks are from IRQ! This will add a custom command to the module, see pp_custom_command_list_element_t!

   private:
    // base working code
    static bool i2c_slave_callback_ISR(struct i2c_slave_device_t* dev, I2CSlaveCallbackReason reason);
    static std::vector<uint8_t> on_send_ISR();
    static void on_command_ISR(Command command, std::vector<uint8_t> additional_data);
    static uint8_t addr;  // my i2c address
    static i2c_slave_device_t* slave_device;
    static QueueHandle_t slave_queue;
    static uint32_t module_version;
    static char module_name[20];

    static volatile Command command_state;  // current command
    static volatile uint16_t app_counter;   // for transfer
    static volatile uint16_t app_transfer_block;

    static std::vector<app_list_element_t> app_list;
    static std::vector<pp_custom_command_list_element_t> custom_command_list;

    // callback pointers
    static get_features_CB features_cb;
    static get_gps_data_CB gps_data_cb;
    static get_orientation_data_CB orientation_data_cb;
    static get_environment_data_CB environment_data_cb;
    static get_light_data_CB light_data_cb;
    static get_shell_data_size_CB shell_data_size_cb;
    static got_shell_data_CB got_shell_data_cb;
    static send_shell_data_CB send_shell_data_cb;
};

class ChipFeatures {
   private:
    uint64_t features;  // Store the features as bitmask

   public:
    ChipFeatures()
        : features(static_cast<uint64_t>(SupportedFeatures::FEAT_NONE)) {}

    void reset() {
        features = static_cast<uint64_t>(SupportedFeatures::FEAT_NONE);
    }

    // Enable a feature
    void enableFeature(SupportedFeatures feature) {
        features |= static_cast<uint64_t>(feature);
    }

    // Disable a feature
    void disableFeature(SupportedFeatures feature) {
        features &= ~static_cast<uint64_t>(feature);
    }

    // Check if a feature is enabled
    bool hasFeature(SupportedFeatures feature) const {
        return (features & static_cast<uint64_t>(feature)) != 0;
    }

    // Toggle a feature on/off
    void toggleFeature(SupportedFeatures feature) {
        features ^= static_cast<uint64_t>(feature);
    }

    uint64_t getFeatures() const {
        return features;
    }
};