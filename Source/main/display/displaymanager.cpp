
#include "displaymanager.hpp"
#include "display_ssd1306.hpp"

bool DisplayManager::init() {
    uint8_t i2caddr = 0;

    // todo init ws display

    // check for sd1306 display
    i2caddr = getDevAddr(SSD1306);
    if (i2caddr != 0) {
        ESP_LOGI("DisplayManager", "SSD1306 display found!");
        displayMain = new Display_Ssd1306();
        if (displayMain->init(i2caddr)) {
            ESP_LOGI("DisplayManager", "SSD1306 display initialized successfully.");
            return true;
        } else {
            ESP_LOGE("DisplayManager", "Failed to initialize SSD1306 display.");
            delete displayMain;  // Clean up if initialization failed
            displayMain = nullptr;
        }
    }

    return false;  // No display found
}

void DisplayManager::setDirty() {
    isDirty = true;
}

void DisplayManager::loop(uint32_t currentMillis) {
    if (currentMillis - time_last_millis_hit < 1000) {
        return;  // avoid too fast loops
    }
    // from here the code hits around 1 seconds
    time_last_millis_hit = currentMillis;

    if (displayMain == nullptr && displayWs == nullptr) {
        return;  // Nothing to do
    }

    // check if rotation needed
    if (selectedScreen == SCREEN_ROTATE) {
        rotationTimer++;
        if (rotationTimer >= rotationSpeed) {
            rotationTimer = 0;
            currDispScreen++;
            if (currDispScreen >= SCREEN_MAX) {
                currDispScreen = SCREEN_MAIN_INFO;  // reset to main info
            }
            isDirty = true;
        }
    }

    // check if draw is needed
    if (isDirty) {        // maybe needs different level of dirtyness, to avoid redwar everything
        isDirty = false;  // reset dirty flag
        switch (currDispScreen) {
            case SCREEN_GPS_INFO:
                DrawGpsInfo(displayMain);
                DrawGpsInfo(displayWs);
                break;
            case SCREEN_SAT_TRACK_INFO:
                DrawSatTrackInfo(displayMain);
                DrawSatTrackInfo(displayWs);
                break;
            case SCREEN_MEASUREMENT_INFO:
                DrawMeasurementInfo(displayMain);
                DrawMeasurementInfo(displayWs);
                break;
            case SCREEN_PP_DATA:
                DrawPPData(displayMain);
                DrawPPData(displayWs);
                break;
            default:
            case SCREEN_MAIN_INFO:
                DrawMainInfo(displayMain);
                DrawMainInfo(displayWs);
                break;
        }
    }
}

void DisplayManager::DrawMainInfo(DisplayGeneric* display) {
    if (display == nullptr) return;
    display->clear();
    display->showTitle("Main Info");
    std::string mainText = "Wifi: " + std::string(state_wifi ? "+" : "-") + "\n" +
                           "AP: " + std::string(state_wifi_ap ? "+" : "-") + "\n" +
                           "GPS: " + std::string(state_gps ? "+" : "-") + "\n" +
                           "PP: " + std::string(state_pp ? "+" : "-");
    display->showMainTextMultiline(mainText);
    display->draw();
}

void DisplayManager::DrawGpsInfo(DisplayGeneric* display) {
    if (display == nullptr) return;
    display->clear();
    display->showTitle("GPS Info");
    std::string gpsText = "";
    if (!gpsdata || (gpsdata->latitude == 200 && gpsdata->longitude == 200) || (gpsdata->latitude == 0 && gpsdata->longitude == 0)) {
        gpsText = "No GPS data.";
    } else {
        char latStr[16], lonStr[16], altStr[16];
        snprintf(latStr, sizeof(latStr), "%.5f", gpsdata->latitude);
        snprintf(lonStr, sizeof(lonStr), "%.5f", gpsdata->longitude);
        snprintf(altStr, sizeof(altStr), "%.1f", gpsdata->altitude);
        gpsText = "Lat: " + std::string(latStr) + "\n" +
                  "Lon: " + std::string(lonStr) + "\n" +
                  "Alt: " + std::string(altStr) + " m\n" +
                  "Sats: " + std::to_string(gpsdata->sats_in_use);
    }
    display->showMainTextMultiline(gpsText);
    display->draw();
}

void DisplayManager::DrawSatTrackInfo(DisplayGeneric* display) {
    if (display == nullptr) return;
    display->clear();
    display->showTitle("Satellite Tracking Info");

    display->draw();
}

void DisplayManager::DrawMeasurementInfo(DisplayGeneric* display) {
    if (display == nullptr) return;
    display->clear();
    display->showTitle("Measurement Info");

    display->draw();
}
void DisplayManager::DrawPPData(DisplayGeneric* display) {
    if (display == nullptr) return;
    display->clear();
    display->showTitle("PP Data");

    display->draw();
}