#include "appmanager.hpp"
void SetDisplayDirtyMain();
EPApp* AppManager::currentApp = nullptr;

void AppManager::startApp(AppList app) {
    if (currentApp != nullptr) {
        delete currentApp;  // Clean up previous app
        currentApp = nullptr;
    }
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
    SetDisplayDirtyMain();
}

void AppManager::loop(uint32_t currentMillis) {
    if (currentApp) {
        currentApp->Loop(currentMillis);
    }
}

bool AppManager::handlePPData(std::string& data) {
    // todo check if the data is for me
    if (currentApp) {
        return currentApp->OnPPData(data);
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