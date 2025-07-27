#ifndef APPMANAGER_HPP
#define APPMANAGER_HPP

#include "ep_app.hpp"

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

    static bool handlePPData(std::string& data);

    static bool handleWebData(const char* data, size_t len);

    static void handleDisplayRequest(DisplayGeneric* display);

   private:
    static EPApp* currentApp;
};

#endif  // APPMANAGER_HPP