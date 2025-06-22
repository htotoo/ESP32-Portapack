#include "display_ssd1306.hpp"

bool Display_Ssd1306::init(uint8_t addr) {
    ssd1306_config_t dev_cfg = I2C_SSD1306_128x64_CONFIG_DEFAULT;

    dev_cfg.i2c_address = addr;

    i2c_dev_t i2c0_bus_hdl = {};
    ssd1306_init_desc(&i2c0_bus_hdl, addr, I2C_NUM_0, (gpio_num_t)CONFIG_IC2SDAPIN, (gpio_num_t)CONFIG_IC2SCLPIN);
    ssd1306_init(i2c0_bus_hdl, &dev_cfg, &dev_hdl);
    if (dev_hdl == NULL) {
        ESP_LOGE("Display_Ssd1306", "Failed to initialize SSD1306 display at address 0x%02X", addr);
        return false;
    }
    ssd1306_clear_display(dev_hdl, false);
    ssd1306_set_contrast(dev_hdl, 0xff);
    ssd1306_display_text(dev_hdl, 1, "ESP32-PP", false);

    return true;
};

void Display_Ssd1306::clear() {
    if (dev_hdl != NULL) {
        ssd1306_clear_display(dev_hdl, false);
    }
}

void Display_Ssd1306::showTitle(const std::string& title) {
    if (dev_hdl != NULL) {
        ssd1306_display_text(dev_hdl, 2, title.c_str(), false);
    }
}

void Display_Ssd1306::showMainText(const std::string& text) {
    if (dev_hdl != NULL) {
        ssd1306_display_text(dev_hdl, 4, text.c_str(), false);
    }
}

void Display_Ssd1306::draw() {
    if (dev_hdl != NULL) {
        ssd1306_display_pages(dev_hdl);
    }
}