#ifndef EP_APP_WIFISPAM_HPP
#define EP_APP_WIFISPAM_HPP

// Based on https://github.com/ubermood/UberMayhemESP32/blob/main/Source/lib/UberMayhem/UberPayload.cpp

#include "ep_app.hpp"
#include "esp_random.h"

#define APP_1_PRE_STR "#$$#$$$1"

#define OFFSET_BEACON_DST_ADDR 4
#define OFFSET_BEACON_SRC_ADDR 10
#define OFFSET_BEACON_BSSID 16
#define OFFSET_BEACON_SEQNUM 22
#define OFFSET_BEACON_TIMESTAMP 24
#define OFFSET_BEACON_SSID 38

class EPAppWifiSpam : public EPApp {
   public:
    bool OnPPData(uint16_t command, std::vector<uint8_t>& data) override {
        // Handle PP data specific to WiFi spam app
        return false;
    }

    bool OnPPReqData(uint16_t command, std::vector<uint8_t>& data) override {
        // Handle PP request data specific to WiFi spam app
        return false;
    }

    bool OnWebData(std::string& data) override;

    void OnDisplayRequest(DisplayGeneric* display) override;
    void Loop(uint32_t currentMillis) override;

   private:
    void sendBeacon(std::string ssid, uint8_t macid);
    void randomizeBeaconSrcMac(uint8_t* beacon_raw, uint8_t macid);
    uint8_t current_mode = 0;  // standby, 1 = random chars, 2 = Rick Roll
    uint8_t ricknum = 0;
    uint32_t lastBeaconTime = 0;

    uint8_t beacon_raw[90] = {
        0x80,
        0x00,
        0x00,
        0x00,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x64,
        0x00,
        0x31,
        0x04,
        0x00,
        0x00,
        0x01,
        0x08,
        0x82,
        0x84,
        0x8b,
        0x96,
        0x24,
        0x30,
        0x48,
        0x6c,
        0x03,
        0x01,
        0x06,
        0x05,
        0x04,
        0x01,
        0x02,
        0x00,
        0x00,
        0x30,
        0x18,
        0x01,
        0x00,
        0x00,
        0x0f,
        0xac,
        0x02,
        0x02,
        0x00,
        0x00,
        0x0f,
        0xac,
        0x04,
        0x00,
        0x0f,
        0xac,
        0x04,
        0x01,
        0x00,
        0x00,
        0x0f,
        0xac,
        0x02,
        0x00,
        0x00,
    };

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

    uint8_t beacon[sizeof(beacon_raw) + 32];
};

#endif  // EP_APP_WIFISPAM_HPP