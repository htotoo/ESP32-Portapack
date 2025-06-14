#ifndef _DISPLAY_SKELETON_HPP_
#define _DISPLAY_SKELETON_HPP_
#include <stdint.h>
#include <string>
#include "esp_log.h"

class DisplayGeneric {
   public:
    virtual bool init(uint8_t addr) = 0;  // Initialize the display
    virtual ~DisplayGeneric() = default;

   protected:
    uint8_t addr = 0;             // 0 = none, 255 = spi, 254 = spec, like WS
    bool is_initialized = false;  // Flag to check if the display is initialized
};

#endif