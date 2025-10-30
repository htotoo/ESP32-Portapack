#ifndef EP_APP_WIFISPAM_HPP
#define EP_APP_WIFISPAM_HPP

// Based on https://github.com/ubermood/UberMayhemESP32/blob/main/Source/lib/UberMayhem/UberPayload.cpp and  https://github.com/justcallmekoko/ESP32Marauder/blob/c16afc958b41881342fe810892988efb54d1a0de/esp32_marauder/WiFiScan.cpp#L6172

#include "ep_app.hpp"
#include "esp_random.h"

#define APP_1_PRE_STR "#$$#$$$1"

class EPAppWifiSpam : public EPApp {
   public:
    bool OnPPData(uint16_t command, std::vector<uint8_t>& data) override;
    bool OnPPReqData(uint16_t command, std::vector<uint8_t>& data) override;

    bool OnWebData(std::string& data) override;

    void OnDisplayRequest(DisplayGeneric* display) override;
    void Loop(uint32_t currentMillis) override;

   private:
    void sendBeacon(std::string ssid, uint8_t macid);
    void randomizeBeaconSrcMac(uint8_t* beacon_raw, uint8_t macid);
    uint8_t current_mode = 0;  // standby, 1 = random chars, 2 = Rick Roll
    uint8_t ricknum = 0;
    uint32_t lastBeaconTime = 0;

    uint8_t packet[128] = {0x80, 0x00, 0x00, 0x00,                                 // Frame Control, Duration
                           /*4*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,               // Destination address
                           /*10*/ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // Source address - overwritten later
                           /*16*/ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              // BSSID - overwritten to the same as the source address
                           /*22*/ 0xc0, 0x6c,                                      // Seq-ctl
                           /*24*/ 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00,  // timestamp - the number of microseconds the AP has been active
                           /*32*/ 0x64, 0x00,                                      // Beacon interval
                           /*34*/ 0x01, 0x04,                                      // Capability info
                                                                                   /* SSID */
                           /*36*/ 0x00};

    const char* rick_ssids[8] = {
        "01 - Never gonna give you up",
        "02 - Never gonna let you down",
        "03 - Never gonna run around",
        "04 - and desert you",
        "05 - Never gonna make you cry",
        "06 - Never gonna say goodbye",
        "07 - Never gonna tell a lie",
        "08 - and hurt you"};
    static constexpr char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
};

#endif  // EP_APP_WIFISPAM_HPP