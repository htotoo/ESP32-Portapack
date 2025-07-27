#include "appmanager.hpp"

void SetDisplayDirtyMain();
EPApp* AppManager::currentApp = nullptr;
uint16_t AppManager::currentAppId = 0;

void AppManager::startApp(AppList app) {
    stopApp();
    switch (app) {
        case AppList::WIFISPAM:
            currentAppId = (uint16_t)app;
            break;
        case AppList::WIFILIST:
            currentAppId = (uint16_t)app;
            break;
        case AppList::WIFIPROBESNIFFER:
            currentAppId = (uint16_t)app;
            break;
        case AppList::NONE:
        default:
            // already stopped
            break;
    }
    sendCurrentAppToWeb();
}

void AppManager::loop(uint32_t currentMillis) {
    if (currentApp) {
        currentApp->Loop(currentMillis);
    }
}

// IRQ CALLBACK!!!!!
bool AppManager::handlePPData(uint16_t command, std::vector<uint8_t>& data) {
    // todo check if the data is for me
    if (currentApp) {
        return currentApp->OnPPData(command, data);
    }
    return false;
}

// IRQ CALLBACK!!!!!
bool AppManager::handlePPReqData(uint16_t command, std::vector<uint8_t>& data) {
    // todo check if the data is for me
    if (currentApp) {
        return currentApp->OnPPReqData(command, data);
    }
    return false;
}

void AppManager::sendCurrentAppToWeb() {
    std::string capidresult = APPMGR_PRE_STR;
    if (currentAppId < 10) {
        capidresult += "0";  // add leading zero for single digit app ids
    }
    capidresult += std::to_string((uint16_t)currentAppId);
    capidresult += "\r\n";
    ws_sendall((uint8_t*)capidresult.c_str(), capidresult.size(), false);
}

bool AppManager::handleWebData(const char* data, size_t len) {
    // check if the data is for me
    if (len >= 9) {
        ESP_LOGI("AppManager", "Web data: %.*s", (int)len, data);
        // #$##$$$99
        if (strncmp(data, APPMGR_PRE_STR, 7) == 0) {
            // this could be mine start / stop signal
            uint16_t appid = (uint16_t)(((data[7] - '0') * 10) + (data[8] - '0'));
            ESP_LOGI("AppManager", "MATCHED APPMGR_PRE_STR. appid: %d", appid);
            if (appid == 0) {
                stopApp();  // stop app
                return true;
            } else if (appid < (uint16_t)AppList::MAX) {
                startApp((AppList)appid);
                return true;  // start app
            } else {
                if (appid == 99) {
                    // requesting for the currently running app's id
                    sendCurrentAppToWeb();
                }
                return true;  // unknown app id, but we handled it
            }
        }
    }

    if (currentApp) {
        std::string strData(data, len);
        return currentApp->OnWebData(strData);
    }
    return false;
}

void AppManager::handleDisplayRequest(DisplayGeneric* display) {
    if (currentApp) {
        currentApp->OnDisplayRequest(display);
    } else {
        display->showTitle("App");
        display->showMainText("No app running");
    }
}

void AppManager::stopApp() {
    if (currentApp) {
        delete currentApp;
        currentApp = nullptr;
    }
    currentAppId = 0;
    SetDisplayDirtyMain();
    sendCurrentAppToWeb();
}