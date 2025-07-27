#include "appmanager.hpp"

void SetDisplayDirtyMain();
EPApp* AppManager::currentApp = nullptr;

void AppManager::startApp(AppList app) {
    stopApp();
    switch (app) {
        case AppList::WIFISPAM:

            break;
        case AppList::WIFILIST:

            break;
        case AppList::WIFIPROBESNIFFER:

            break;
        case AppList::NONE:
        default:
            currentApp = nullptr;
            break;
    }
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

bool AppManager::handleWebData(const char* data, size_t len) {
    // todo check if the data is for me
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
    SetDisplayDirtyMain();
}