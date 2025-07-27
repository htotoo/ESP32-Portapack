#ifndef EP_APP_HPP
#define EP_APP_HPP

#include <stdint.h>
#include <string>
#include "esp_log.h"
#include "ppshellcomm.h"
#include "display/displayskeleton.hpp"

bool ws_sendall(uint8_t* data, size_t len, bool asyncmsg);
void SetDisplayDirtyMain();

class EPApp {
   public:
    virtual ~EPApp() = 0;

    virtual bool OnPPData(std::string& data);
    virtual bool OnWebData(std::string& data);
    virtual void OnDisplayRequest(DisplayGeneric* display) {};
    virtual void Loop(uint32_t currentMillis) {};

   protected:
    bool SendDataToWeb(const std::string& data) {
        return ws_sendall((uint8_t*)data.c_str(), data.size(), true);
    }
    bool SendDataToPP(const std::string& data) {
        if (PPShellComm::wait_till_sending(50)) {
            PPShellComm::write_blocking((uint8_t*)data.c_str(), data.size(), true, false);
        }
        return false;
    }
    void SetDisplayDirty() {
        SetDisplayDirtyMain();
    }
};

#endif  // EP_APP_HPP