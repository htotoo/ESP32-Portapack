#ifndef EP_APP_DEAUTH_HPP
#define EP_APP_DEAUTH_HPP

#include "ep_app.hpp"
#include <esp_wifi.h>
#include <vector>
#include <cstring>

#define EP_DEAUTH_MAX_APS 48

// One scanned AP shown for target selection (40 bytes). Mirrored on the PP side.
typedef struct __attribute__((packed)) {
    uint8_t bssid[6];
    char ssid[32];
    uint8_t channel;
    int8_t rssi;
} deauth_ap_t;

// Standalone deauthentication tool: scan APs, pick a target, flood deauth to
// disconnect its clients (awareness / DoS demo). Not a capture tool.
class EPAppDeauth : public EPApp {
   public:
    EPAppDeauth();
    ~EPAppDeauth() override;

    void Loop(uint32_t currentMillis) override;
    bool OnPPData(uint16_t command, std::vector<uint8_t>& data) override;     // IRQ
    bool OnPPReqData(uint16_t command, std::vector<uint8_t>& data) override;  // IRQ

   private:
    void performScan();
    void sendDeauth();

    static EPAppDeauth* instance;
    static deauth_ap_t aps[EP_DEAUTH_MAX_APS];
    static volatile uint16_t apCount;
    static volatile bool scanning;
    static volatile bool attacking;
    static volatile uint16_t deauthSent;

    volatile bool rescanReq = true;
    volatile bool haveTarget = false;
    uint8_t targetBssid[6] = {0};
    uint8_t targetChannel = 1;
    uint16_t reqIdx = 0;
    uint8_t curChannel = 1;
};

#endif  // EP_APP_DEAUTH_HPP
