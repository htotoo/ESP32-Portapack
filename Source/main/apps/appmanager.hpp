#ifndef APPMANAGER_HPP
#define APPMANAGER_HPP

#include "ep_app.hpp"

#define APPMGR_PRE_STR "#$##$$$"

// Enum for the list of applications, please keep the order. This is used in several different places.
enum class AppList {
    NONE,
    WIFISPAM,
    WIFILIST,
    WIFIPROBESNIFFER,
    MAX
};

class AppManager {
   public:
    static void init() {
        currentApp = nullptr;
    }

    static void startApp(AppList app);

    static void stopApp();

    static void loop(uint32_t currentMillis);

    static EPApp* getCurrentApp() { return currentApp; }

    static bool handlePPAppmgrCommands(std::vector<uint8_t>& data);  // this starts the given app. uint16_t appid. if nothing is given (0 bytes, not 0 value!) it is a "what is running" request. 0 value == stop app
    static bool handlePPReqAppmgrCommands(std::vector<uint8_t>& data);

    static bool handlePPData(uint16_t command, std::vector<uint8_t>& data);
    static bool handlePPReqData(uint16_t command, std::vector<uint8_t>& data);

    static bool handleWebData(const char* data, size_t len);

    static void handleDisplayRequest(DisplayGeneric* display);

    static void sendCurrentAppToWeb();

   private:
    static EPApp* currentApp;
    static uint16_t currentAppId;
};

#endif  // APPMANAGER_HPP