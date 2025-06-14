
#include "displaymanager.hpp"

bool DisplayManager::init() {
    uint8_t i2caddr = 0;

    // check for sd1306 display
    i2caddr = getDevAddr(SSD1306);
    if (i2caddr != 0) {
        ESP_LOGE("DisplayManager", "SSD1306 display found!");
        return true;  // No display found
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
        }
    }

    if (isDirty) {        // maybe needs different level of dirtyness, to avoid redwar everything
        isDirty = false;  // reset dirty flag
        if (displayMain != nullptr) {
            ;  // todo
        }
        if (displayWs != nullptr) {
            ;  // todo
        }
    }
}