#ifndef APPMANAGER_HPP
#define APPMANAGER_HPP

#include "ep_app.hpp"

class AppManager {
   public:
    static void init() {
        currentApp = nullptr;
    }

    static EPApp* getCurrentApp() {
        return currentApp;
    }

    static bool handlePPData(std::string& data) {
        if (currentApp) {
            return currentApp->OnPPData(data);
        }
        return false;
    }

    static bool handleWebData(std::string& data) {
        if (currentApp) {
            return currentApp->OnWebData(data);
        }
        return false;
    }

    static void handleDisplayRequest(DisplayGeneric* display) {
        if (currentApp) {
            currentApp->OnDisplayRequest(display);
        } else {
            display->showTitle("App");
            display->showMainText("No app running");
        }
    }

   private:
    static EPApp* currentApp;
};

#endif  // APPMANAGER_HPP