#ifndef DISPLAYMANAGER_HPP
#define DISPLAYMANAGER_HPP

#include <stdint.h>
#include <string>

#include "esp_log.h"
#include "displayskeleton.hpp"
#include "../sensordb.h"
#include "../ppi2c/pp_structures.hpp"
#include "../wifim.h"

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
    void setGpsDataSource(ppgpssmall_t* gpsdata) {
        this->gpsdata = gpsdata;
        if (currDispScreen == SCREEN_GPS_INFO) {
            isDirty = true;  // mark display as dirty to update
        }
    }
    void setOrientationDataSource(orientation_t* orientationdata) {
        this->orientationdata = orientationdata;
        if (currDispScreen == SCREEN_GPS_INFO) {
            isDirty = true;  // mark display as dirty to update
        }
    }
    void setEnvironmentDataSource(environment_t* environmentdata) {
        this->environmentdata = environmentdata;
        if (currDispScreen == SCREEN_MEASUREMENT_INFO) {
            isDirty = true;  // mark display as dirty to update
        }
    }

    void setLightDataSource(uint16_t* lightdata) {
        this->lightdata = lightdata;
        if (currDispScreen == SCREEN_MEASUREMENT_INFO) {
            isDirty = true;  // mark display as dirty to update
        }
    }
    void setSatTrackDataSource(sattrackdata_t* sattrackdata, std::string* sattrackname) {
        this->sattrackdata = sattrackdata;
        this->sattrackname = sattrackname;
        if (currDispScreen == SCREEN_SAT_TRACK_INFO) {
            isDirty = true;  // mark display as dirty to update
        }
    }

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
    uint8_t currDispScreen = SCREEN_ROTATE;
    uint8_t rotationSpeed = 5;  // how many seconds to wait before rotating the screen
    uint8_t rotationTimer = 0;  // sec since last rotation

    bool isDirty = false;  // flag to check if the display needs to be updated

    DisplayGeneric* displayMain = nullptr;  // Main display
    DisplayGeneric* displayWs = nullptr;    // WebSocket virtual display //todo

    bool state_wifi = false;
    bool state_wifi_ap = false;
    bool state_gps = false;
    bool state_pp = false;

    ppgpssmall_t* gpsdata = nullptr;
    orientation_t* orientationdata = nullptr;
    environment_t* environmentdata = nullptr;
    uint16_t* lightdata = nullptr;
    sattrackdata_t* sattrackdata = nullptr;
    std::string* sattrackname = nullptr;
};

#endif  // DISPLAYMANAGER_HPP