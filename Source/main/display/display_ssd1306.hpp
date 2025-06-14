#ifndef DISPLAY_SSD1306_HPP
#define DISPLAY_SSD1306_HPP

#include "displayskeleton.hpp"

class Display_Ssd1306 : public DisplayGeneric {
   public:
    bool init(uint8_t addr) override;
};

#endif  // DISPLAY_SSD1306_HPP