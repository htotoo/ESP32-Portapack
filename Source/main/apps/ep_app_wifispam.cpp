#include "ep_app_wifispam.hpp"
#include <esp_wifi.h>
// Bypass 802.11 RAW frames sanity check
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    return 0;
}  // -Wl,-zmuldefs

void EPAppWifiSpam::randomizeBeaconSrcMac(uint8_t* beacon_raw, uint8_t macid) {
    if (macid == 0) {
        uint8_t random_byte;
        random_byte = (esp_random() % 13) + 1;
        beacon_raw[50] = random_byte;  // Channel
        for (int i = 0; i < 6; i++) {
            random_byte = esp_random() & 0xFF;
            beacon_raw[10 + i] = random_byte;  // Source Address (bytes 10–15)
            beacon_raw[16 + i] = random_byte;  // BSSID (bytes 16–21)
        }
    } else {
        beacon_raw[10 + 0] = beacon_raw[16 + 0] = 0xab;
        beacon_raw[10 + 1] = beacon_raw[16 + 1] = 0xba;
        beacon_raw[10 + 2] = beacon_raw[16 + 2] = 0xde;
        beacon_raw[10 + 3] = beacon_raw[16 + 3] = 0xad;
        beacon_raw[10 + 4] = beacon_raw[16 + 4] = 0xbf;
        beacon_raw[10 + 5] = beacon_raw[16 + 5] = macid;
    }
    // Ensure the MAC address is valid (set locally administered bit, clear multicast bit)
    beacon_raw[10] = (beacon_raw[10] & 0xFE) | 0x02;
    beacon_raw[16] = beacon_raw[10];  // First byte of BSSID matches Source Address
}

void EPAppWifiSpam::sendBeacon(std::string ssid, uint8_t macid) {
    randomizeBeaconSrcMac(beacon_raw, macid);
    // Build the beacon frame
    memcpy(beacon, beacon_raw, OFFSET_BEACON_SSID - 1);

    beacon[OFFSET_BEACON_SSID - 1] = ssid.size();
    memcpy(&beacon[OFFSET_BEACON_SSID], ssid.data(), ssid.size());
    memcpy(&beacon[OFFSET_BEACON_SSID + ssid.size()], &beacon_raw[OFFSET_BEACON_SSID], 90 - OFFSET_BEACON_SSID);
    // Send 12-bit random sequence number
    uint16_t seq = esp_random() & 0xFFF;
    beacon[OFFSET_BEACON_SEQNUM] = (seq << 4) & 0xF0;
    beacon[OFFSET_BEACON_SEQNUM + 1] = (seq >> 4) & 0xFF;

    esp_wifi_80211_tx(WIFI_IF_AP, beacon, 90 + ssid.size(), false);
}

void EPAppWifiSpam::Loop(uint32_t currentMillis) {
    if (current_mode == 0) return;  // Standby mode, do nothing
    if (currentMillis - lastBeaconTime > 15) {
        if (current_mode == 1) {  // Random characters mode
            std::string ssid;
            uint8_t len = esp_random() % 31 + 1;
            for (int i = 0; i < len; i++) {
                ssid += charset[esp_random() % (sizeof(charset) - 1)];
            }
            sendBeacon(ssid, 0);  // Use macid 0 for random mode
        }
        if (current_mode == 2) {  // Rick Roll mode
            sendBeacon(std::string(rick_ssids[ricknum]), ricknum % 8 + 1);
            ricknum = (ricknum + 1) % 8;
        }
        lastBeaconTime = currentMillis;
    }
}

void EPAppWifiSpam::OnDisplayRequest(DisplayGeneric* display) {
    display->showTitle("WiFi Spam App");
    if (current_mode == 0) {
        display->showMainText("Mode: Standby");
    } else if (current_mode == 1) {
        display->showMainText("Mode: Random Chars");
    } else if (current_mode == 2) {
        display->showMainText("Mode: Rick Roll");
    }
}

bool EPAppWifiSpam::OnWebData(std::string& data) {
    if (data.compare(APP_1_PRE_STR "0\r\n") == 0) {
        current_mode = 0;
        SetDisplayDirty();
        return true;
    }
    if (data.compare(APP_1_PRE_STR "1\r\n") == 0) {
        current_mode = 1;
        SetDisplayDirty();
        return true;
    }
    if (data.compare(APP_1_PRE_STR "2\r\n") == 0) {
        current_mode = 2;
        SetDisplayDirty();
        return true;
    }
    return false;
}