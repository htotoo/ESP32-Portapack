#include "appmanager.hpp"

#include "ep_app_wifispam.hpp"
#include "ep_app_evilportal.hpp"
#include "ep_app_wifianalyzer.hpp"
#include "ep_app_probesniffer.hpp"
#include "ep_app_pmkid.hpp"
#include "ep_app_deauth.hpp"
#include "ep_app_blescan.hpp"
#include "ep_app_blespam.hpp"
#include "ep_app_drone.hpp"

void SetDisplayDirtyMain();
EPApp* AppManager::currentApp = nullptr;
uint16_t AppManager::currentAppId = 0;
volatile bool AppManager::pendingChange = false;
volatile uint16_t AppManager::pendingAppId = 0;

void AppManager::startApp(AppList app) {
    stopApp();
    switch (app) {
        case AppList::WIFISPAM:
            currentApp = new EPAppWifiSpam();
            currentAppId = (uint16_t)app;
            break;
        case AppList::WIFILIST:
            currentAppId = (uint16_t)app;
            break;
        case AppList::WIFIPROBESNIFFER:
            currentApp = new EPAppProbeSniffer();
            currentAppId = (uint16_t)app;
            break;
        case AppList::EVILPORTAL:
            currentApp = new EPAppEvilPortal();
            currentAppId = (uint16_t)app;
            break;
        case AppList::WIFIANALYZER:
            currentApp = new EPAppWifiAnalyzer();
            currentAppId = (uint16_t)app;
            break;
        case AppList::PMKIDCAPTURE:
            currentApp = new EPAppPmkid();
            currentAppId = (uint16_t)app;
            break;
        case AppList::DEAUTH:
            currentApp = new EPAppDeauth();
            currentAppId = (uint16_t)app;
            break;
        case AppList::BLESCAN:
            currentApp = new EPAppBleScan();
            currentAppId = (uint16_t)app;
            break;
        case AppList::BLESPAM:
            currentApp = new EPAppBleSpam();
            currentAppId = (uint16_t)app;
            break;
        case AppList::DRONE:
            currentApp = new EPAppDrone();
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
    // Execute any queued start/stop in task context (safe for new/delete and
    // the app's httpd/socket teardown).
    if (pendingChange) {
        pendingChange = false;
        startApp((AppList)pendingAppId);
    }
    if (currentApp) {
        currentApp->Loop(currentMillis);
    }
}

// IRQ CALLBACK!!!!! Only queue the request; loop() performs it.
bool AppManager::handlePPAppmgrCommands(std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    pendingAppId = *(uint16_t*)data.data();
    pendingChange = true;
    return true;
}

// IRQ CALLBACK!!!!!
bool AppManager::handlePPReqAppmgrCommands(std::vector<uint8_t>& data) {
    data.resize(2);
    *(uint16_t*)data.data() = currentAppId;
    // ESP_DRAM_LOGW("Appmgr", "get appmgrcmd: %u", currentAppId);
    return true;
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
                pendingAppId = 0;  // stop app (deferred to loop/main task)
                pendingChange = true;
                return true;
            } else if (appid < (uint16_t)AppList::MAX) {
                pendingAppId = appid;  // start app (deferred to loop/main task)
                pendingChange = true;
                return true;
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