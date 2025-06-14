#ifndef DISPLAYMANAGER_HPP
#define DISPLAYMANAGER_HPP

#include <stdint.h>
#include <string>

#include "esp_log.h"
#include "displayskeleton.hpp"
#include "../sensordb.h"

enum ScreenType : uint8_t {
    SCREEN_ROTATE = 0,
    SCREEN_MAIN_INFO,
    SCREEN_GPS_INFO,
    SCREEN_SAT_TRACK_INFO,
    SCREEN_MEASUREMENT_INFO,
    SCREEN_PP_DATA,
    SCREEN_MAX
};

class DisplayManager {
   public:
    // Initialize the display manager
    bool init();
    void loop(uint32_t currentMillis);
    void setDirty();

   private:
    uint32_t time_last_millis_hit = 0;  // when the last loop was called
    uint8_t selectedScreen = SCREEN_ROTATE;
    uint8_t currDispScreen = SCREEN_MAIN_INFO;
    uint8_t rotationSpeed = 5;  // how many seconds to wait before rotating the screen
    uint8_t rotationTimer = 0;  // sec since last rotation

    bool isDirty = false;  // flag to check if the display needs to be updated

    DisplayGeneric* displayMain = nullptr;  // Main display
    DisplayGeneric* displayWs = nullptr;    // WebSocket virtual display //todo
};

#endif  // DISPLAYMANAGER_HPP