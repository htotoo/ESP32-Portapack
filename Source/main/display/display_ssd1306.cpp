#include "display_ssd1306.hpp"

bool Display_Ssd1306::init(uint8_t addr, int sda, int scl) {
    if (sda < 0 || scl < 0) {
        ESP_LOGW("Display_Ssd1306", "I2C pins not set. Skipping SSD1306 initialization.");
        return false;
    }
    ssd1306_config_t dev_cfg = I2C_SSD1306_128x64_CONFIG_DEFAULT;

    dev_cfg.i2c_address = addr;

    i2c_dev_t i2c0_bus_hdl = {};
    ssd1306_init_desc(&i2c0_bus_hdl, addr, I2C_NUM_0, (gpio_num_t)sda, (gpio_num_t)scl);
    ssd1306_init(i2c0_bus_hdl, &dev_cfg, &dev_hdl);
    if (dev_hdl == NULL) {
        ESP_LOGE("Display_Ssd1306", "Failed to initialize SSD1306 display at address 0x%02X", addr);
        return false;
    }
    ssd1306_set_contrast(dev_hdl, 0xff);
    clear();
    showTitle("ESP32-PP");

    return true;
};

void Display_Ssd1306::clear() {
    if (dev_hdl != NULL) {
        ssd1306_clear_display(dev_hdl, false);
    }
}

void Display_Ssd1306::showTitle(const std::string& title) {
    if (dev_hdl != NULL) {
        ssd1306_display_text(dev_hdl, 1, (char*)title.c_str(), false);
    }
}

void Display_Ssd1306::showMainText(const std::string& text) {
    if (dev_hdl != NULL) {
        ssd1306_display_text(dev_hdl, 2, (char*)text.c_str(), false);
    }
}

void Display_Ssd1306::showMainTextMultiline(const std::string& text) {
    if (dev_hdl != NULL) {
        uint8_t cp = 2;  // start at page 2
        size_t text_length = text.length();
        size_t max_length = 16;  // max characters per line
        for (size_t i = 0; i < text_length;) {
            size_t next_newline = text.find('\n', i);
            size_t line_end = (next_newline != std::string::npos && next_newline < i + max_length)
                                  ? next_newline
                                  : std::min(i + max_length, text_length);
            std::string line = text.substr(i, line_end - i);
            ssd1306_display_text(dev_hdl, cp++, (char*)line.c_str(), false);
            i = line_end;
            if (i < text_length && text[i] == '\n') {
                ++i;
            }
        }
    }
}

void Display_Ssd1306::draw() {
    if (dev_hdl != NULL) {
        ssd1306_display_pages(dev_hdl);
    }
}