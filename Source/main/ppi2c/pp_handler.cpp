
#include "pp_handler.hpp"
#include <cstring>
#include "apps/appmanager.hpp"

uint8_t PPHandler::addr = 0;
i2c_slave_device_t* PPHandler::slave_device;
QueueHandle_t PPHandler::slave_queue;
volatile uint16_t PPHandler::command_state = (uint16_t)Command::COMMAND_NONE;
volatile uint16_t PPHandler::app_counter = 0;
volatile uint16_t PPHandler::app_transfer_block = 0;
get_features_CB PPHandler::features_cb = nullptr;
get_gps_data_CB PPHandler::gps_data_cb = nullptr;
get_orientation_data_CB PPHandler::orientation_data_cb = nullptr;
get_environment_data_CB PPHandler::environment_data_cb = nullptr;
get_light_data_CB PPHandler::light_data_cb = nullptr;

get_shell_data_size_CB PPHandler::shell_data_size_cb = nullptr;
got_shell_data_CB PPHandler::got_shell_data_cb = nullptr;
send_shell_data_CB PPHandler::send_shell_data_cb = nullptr;

std::vector<app_list_element_t> PPHandler::app_list;
std::vector<pp_custom_command_list_element_t> PPHandler::custom_command_list;
uint32_t PPHandler::module_version = 1;
char PPHandler::module_name[20] = "ESP32MODULE";

void PPHandler::init(gpio_num_t scl, gpio_num_t sda, uint8_t addr_) {
    addr = addr_;
    i2c_slave_config_t i2c_slave_config = {
        i2c_slave_callback_ISR,
        addr,
        scl,
        sda,
        I2C_NUM_1};

    slave_queue = xQueueCreate(1, sizeof(uint16_t));

    ESP_ERROR_CHECK(i2c_slave_new(&i2c_slave_config, &slave_device));
}

uint32_t PPHandler::get_appCount() {
    return (uint32_t)app_list.size();
}

// region Callback setters

void PPHandler::set_get_features_CB(get_features_CB cb) {
    features_cb = cb;
}

void PPHandler::set_get_gps_data_CB(get_gps_data_CB cb) {
    gps_data_cb = cb;
}

void PPHandler::set_get_orientation_data_CB(get_orientation_data_CB cb) {
    orientation_data_cb = cb;
}

void PPHandler::set_get_environment_data_CB(get_environment_data_CB cb) {
    environment_data_cb = cb;
}

void PPHandler::set_get_light_data_CB(get_light_data_CB cb) {
    light_data_cb = cb;
}

void PPHandler::set_get_shell_data_size_CB(get_shell_data_size_CB cb) {
    shell_data_size_cb = cb;
}

void PPHandler::set_got_shell_data_CB(got_shell_data_CB cb) {
    got_shell_data_cb = cb;
}

void PPHandler::set_send_shell_data_CB(send_shell_data_CB cb) {
    send_shell_data_cb = cb;
}

// endregion

void PPHandler::set_module_name(std::string name) {
    strncpy(module_name, name.c_str(), 20);
    module_name[19] = 0;
}

void PPHandler::set_module_version(uint32_t version) {
    module_version = version;
}

bool PPHandler::add_app(uint8_t* binary, uint32_t size) {
    if (size % 32 != 0 || size < sizeof(standalone_app_info)) {
        esp_rom_printf("FAILED ADDING APP, BAD SIZE\n");
        return false;
    }

    app_list.push_back({binary, size});
    return true;
}

void PPHandler::add_custom_command(uint16_t command, pp_i2c_command got_command, pp_i2c_command send_command) {
    pp_custom_command_list_element_t element;
    element.command = command;
    element.got_command = got_command;
    element.send_command = send_command;
    custom_command_list.push_back(element);
}

// when pp tx-es to us
void PPHandler::on_command_ISR(uint16_t command, std::vector<uint8_t> additional_data) {
    command_state = command;
    bool found = false;
    switch (command) {
        case (uint16_t)Command::COMMAND_APP_INFO:
            if (additional_data.size() == 2)
                app_counter = *(uint16_t*)additional_data.data();
            break;

        case (uint16_t)Command::COMMAND_APP_TRANSFER:
            if (additional_data.size() == 4) {
                app_counter = *(uint16_t*)additional_data.data();
                app_transfer_block = *(uint16_t*)(additional_data.data() + 2);
            }
            break;

        case (uint16_t)Command::COMMAND_GETFEATURE_MASK:
            break;

        case (uint16_t)Command::COMMAND_SHELL_PPTOMOD_DATA:
            if (got_shell_data_cb)
                got_shell_data_cb(additional_data);
            break;
        default:
            for (auto element : custom_command_list) {
                if (element.command == (uint16_t)command) {
                    if (element.got_command) {
                        pp_command_data_t data;
                        data.data = &additional_data;
                        element.got_command(data);
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                AppManager::handlePPData(command, additional_data);
            }
            break;
    }

    BaseType_t high_task_wakeup = pdFALSE;
    xQueueSendFromISR(slave_queue, &command, &high_task_wakeup);
}

bool PPHandler::i2c_slave_callback_ISR(struct i2c_slave_device_t* dev, I2CSlaveCallbackReason reason) {
    switch (reason) {
        case I2C_CALLBACK_REPEAT_START:
            break;

        case I2C_CALLBACK_SEND_DATA:
            if (dev->state == I2C_STATE_SEND) {
                auto data = on_send_ISR();

                if (data.size() == 0)
                    return false;

                if (data.size() > 128) {
                    esp_rom_printf("FAILED SENDING DATA. Byte count of %d is exceeding cache size of 128.\n", data.size());
                    return false;
                }

                uint8_t len = data.size();
                i2c_slave_send_data(dev, data.data(), &len);

                if (len != data.size()) {
                    esp_rom_printf("FAILED SENDING DATA. Tried to send %d bytes, but only %d bytes were sent because of cache limitation.\n", data.size(), len);
                    return false;
                }
            }
            break;

        case I2C_CALLBACK_DONE:
            if (dev->state == I2C_STATE_RECV) {
                uint16_t command = *(uint16_t*)&dev->buffer[dev->bufstart];
                std::vector<uint8_t> additional_data(dev->buffer + dev->bufstart + 2, dev->buffer + dev->bufend);
                on_command_ISR(command, additional_data);
            }
            break;

        default:
            return false;
    }
    return true;
}

// this handle, when the PP needs data
std::vector<uint8_t> PPHandler::on_send_ISR() {
    bool found = false;
    switch (command_state) {
        case (uint16_t)Command::COMMAND_INFO: {
            device_info info = {
                /* api_version = */ PP_API_VERSION,
                /* module_version = */ module_version,
                /* module_name = */ "",
                /* application_count = */ app_list.size()};
            strncpy(info.module_name, module_name, 20);
            return std::vector<uint8_t>((uint8_t*)&info, (uint8_t*)&info + sizeof(info));
        }

        case (uint16_t)Command::COMMAND_APP_INFO: {
            if (app_counter <= app_list.size() - 1) {
                standalone_app_info app_info;
                std::memset(&app_info, 0, sizeof(app_info));
                std::memcpy(&app_info, app_list[app_counter].binary, sizeof(app_info) - 4);
                app_info.binary_size = app_list[app_counter].size;
                app_counter = app_counter + 1;
                return std::vector<uint8_t>((uint8_t*)&app_info, (uint8_t*)&app_info + sizeof(app_info));
            }

            break;
        }

        case (uint16_t)Command::COMMAND_APP_TRANSFER: {
            if (app_counter <= app_list.size() - 1 && app_transfer_block < app_list[app_counter].size / 128) {
                return std::vector<uint8_t>(app_list[app_counter].binary + app_transfer_block * 128, app_list[app_counter].binary + app_transfer_block * 128 + 128);
            }
            break;
        }

        case (uint16_t)Command::COMMAND_GETFEATURE_MASK: {
            uint64_t features = 0;
            if (features_cb)
                features_cb(features);
            else
                features = app_list.size() > 0 ? (uint64_t)SupportedFeatures::FEAT_EXT_APP : (uint64_t)SupportedFeatures::FEAT_NONE;  // default, only check if ext app added or not
            return std::vector<uint8_t>((uint8_t*)&features, (uint8_t*)&features + sizeof(features));
        }

        case (uint16_t)Command::COMMAND_GETFEAT_DATA_GPS: {
            ppgpssmall_t gpsdata = {};
            if (gps_data_cb)
                gps_data_cb(gpsdata);
            return std::vector<uint8_t>((uint8_t*)&gpsdata, (uint8_t*)&gpsdata + sizeof(gpsdata));
        }

        case (uint16_t)Command::COMMAND_GETFEAT_DATA_ORIENTATION: {
            orientation_t ori = {400, 400};  // false data
            if (orientation_data_cb)
                orientation_data_cb(ori);
            return std::vector<uint8_t>((uint8_t*)&ori, (uint8_t*)&ori + sizeof(ori));
        }

        case (uint16_t)Command::COMMAND_GETFEAT_DATA_ENVIRONMENT: {
            environment_t env = {};
            if (environment_data_cb)
                environment_data_cb(env);
            return std::vector<uint8_t>((uint8_t*)&env, (uint8_t*)&env + sizeof(env));
        }

        case (uint16_t)Command::COMMAND_GETFEAT_DATA_LIGHT: {
            uint16_t light = 0;
            if (light_data_cb)
                light_data_cb(light);
            return std::vector<uint8_t>((uint8_t*)&light, (uint8_t*)&light + sizeof(light));
        }

        case (uint16_t)Command::COMMAND_SHELL_MODTOPP_DATA_SIZE: {
            uint16_t size = 0;
            if (shell_data_size_cb)
                size = shell_data_size_cb();
            return std::vector<uint8_t>((uint8_t*)&size, (uint8_t*)&size + sizeof(size));
        }

        case (uint16_t)Command::COMMAND_SHELL_MODTOPP_DATA: {
            std::vector<uint8_t> data;
            if (send_shell_data_cb) {
                bool hasmore = false;
                send_shell_data_cb(data, hasmore);
                uint8_t pre = hasmore ? 0x80 : 0x00;
                pre |= data.size();
                data.insert(data.begin(), pre);
                data.resize(65);
            }
            if (data.size() == 0) {
                return {0xFF};
            }
            return data;
        }

        default:
            for (auto element : custom_command_list) {
                if (element.command == (uint16_t)command_state) {
                    if (element.send_command) {
                        std::vector<uint8_t> tmp;
                        pp_command_data_t data = {&tmp};
                        element.send_command(data);
                        return tmp;
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::vector<uint8_t> tmp;
                if (AppManager::handlePPReqData(command_state, tmp)) {
                    return tmp;
                }
            }
            break;
    }

    return {0xFF};
}