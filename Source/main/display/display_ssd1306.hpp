#ifndef DISPLAY_SSD1306_HPP
#define DISPLAY_SSD1306_HPP

#include "displayskeleton.hpp"
#include "../drivers/ssd1306.h"

class Display_Ssd1306 : public DisplayGeneric {
   public:
    bool init(uint8_t addr, int sda, int scl) override;
    void clear() override;
    void showTitle(const std::string& title) override;
    void showMainText(const std::string& text) override;
    void showMainTextMultiline(const std::string& text) override;
    void draw() override;

   private:
    ssd1306_handle_t dev_hdl;
};

#endif  // DISPLAY_SSD1306_HPP