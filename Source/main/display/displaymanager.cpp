
#include "displaymanager.hpp"
#include "display_ssd1306.hpp"

bool DisplayManager::init() {
    uint8_t i2caddr = 0;

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

    display->draw();
}

void DisplayManager::DrawGpsInfo(DisplayGeneric* display) {
    if (display == nullptr) return;
    display->clear();
    display->showTitle("GPS Info");

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