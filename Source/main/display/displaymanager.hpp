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
    void setEspState(bool wifi, bool wifi_ap, bool gps, bool pp) {
        bool changed = false;
        if (state_wifi != wifi) {
            state_wifi = wifi;
            changed = true;
        }
        if (state_wifi_ap != wifi_ap) {
            state_wifi_ap = wifi_ap;
            changed = true;
        }
        if (state_gps != gps) {
            state_gps = gps;
            changed = true;
        }
        if (state_pp != pp) {
            state_pp = pp;
            changed = true;
        }
        if (changed) {
            isDirty = true;  // mark display as dirty to update
        }
    }

   private:
    void DrawMainInfo(DisplayGeneric* display);
    void DrawGpsInfo(DisplayGeneric* display);
    void DrawSatTrackInfo(DisplayGeneric* display);
    void DrawMeasurementInfo(DisplayGeneric* display);
    void DrawPPData(DisplayGeneric* display);

    uint32_t time_last_millis_hit = 0;  // when the last loop was called
    uint8_t selectedScreen = SCREEN_ROTATE;
    uint8_t currDispScreen = SCREEN_MAIN_INFO;
    uint8_t rotationSpeed = 5;  // how many seconds to wait before rotating the screen
    uint8_t rotationTimer = 0;  // sec since last rotation

    bool isDirty = false;  // flag to check if the display needs to be updated

    DisplayGeneric* displayMain = nullptr;  // Main display
    DisplayGeneric* displayWs = nullptr;    // WebSocket virtual display //todo

    bool state_wifi = false;
    bool state_wifi_ap = false;
    bool state_gps = false;
    bool state_pp = false;
};

#endif  // DISPLAYMANAGER_HPP