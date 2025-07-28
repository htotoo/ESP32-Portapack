#include "ep_app_wifispam.hpp"
#include <esp_wifi.h>
// Bypass 802.11 RAW frames sanity check
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    if (arg == 31337)
        return 1;
    else
        return 0;
}  // -Wl,-zmuldefs

void EPAppWifiSpam::randomizeBeaconSrcMac(uint8_t* beacon_raw, uint8_t macid) {
    if (macid == 0) {
        uint8_t random_byte;
        random_byte = (esp_random() % 13) + 1;
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
    randomizeBeaconSrcMac(packet, macid);
    // Build the beacon frame
    packet[37] = ssid.size();
    memcpy(&packet[38], ssid.data(), ssid.size());
    packet[50 + ssid.size()] = 3;  // set channel

    uint8_t postSSID[13] = {0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c,  // supported rate
                            0x03, 0x01, 0x04 /*DSSS (Current Channel)*/};

    // Add everything that goes after the SSID
    for (int i = 0; i < 12; i++)
        packet[38 + ssid.size() + i] = postSSID[i];

    esp_wifi_80211_tx(WIFI_IF_AP, packet, 128, false);  // sizeof?!
    esp_wifi_80211_tx(WIFI_IF_AP, packet, 128, false);  // sizeof?!
}

void EPAppWifiSpam::Loop(uint32_t currentMillis) {
    if (current_mode == 0) return;  // Standby mode, do nothing
    if (currentMillis - lastBeaconTime > 10) {
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
        if (current_mode == 3) {  // Emoji spam mode
            std::string ssid = "";
            uint8_t len = esp_random() % 5 + 1;
            for (int i = 0; i < len; i++) {
                uint8_t emoji_type = esp_random() % 4;  // Choose from 4 different emoji categories
                switch (emoji_type) {                   // a lot may fail, maybe need a better solution, like a list of emojis
                    case 0:
                        // Add a random emoji from U+1F600 (😀) to U+1F636 (😶)
                        {
                            uint8_t emoji_offset = esp_random() % (0xB7 - 0x80);  // 0xB6 - 0x80 + 1 = 0x37
                            char emoji[5] = {0};
                            emoji[0] = '\xF0';
                            emoji[1] = '\x9F';
                            emoji[2] = '\x98';
                            emoji[3] = static_cast<char>(0x80 + emoji_offset);
                            ssid += emoji;
                        }
                        break;
                    case 1: {
                        // Add a random emoji from U+1F601 (😁) to U+1F64F (🙏)
                        uint32_t emoji_start = 0x1F601;
                        uint32_t emoji_end = 0x1F64F;
                        uint32_t emoji_code = emoji_start + (esp_random() % (emoji_end - emoji_start + 1));
                        char emoji[5] = {0};
                        // Encode as UTF-8
                        emoji[0] = 0xF0;
                        emoji[1] = 0x9F;
                        emoji[2] = static_cast<char>(0x80 | ((emoji_code >> 6) & 0x3F));
                        emoji[3] = static_cast<char>(0x80 | (emoji_code & 0x3F));
                        // For code points above U+1F600, adjust the second byte
                        if (emoji_code >= 0x1F600) {
                            emoji[1] = 0x9F;
                            emoji[2] = static_cast<char>(0x80 | ((emoji_code >> 6) & 0x3F));
                            emoji[3] = static_cast<char>(0x80 | (emoji_code & 0x3F));
                        }
                        ssid += emoji;
                    } break;
                    case 2: {
                        // Add a random emoji from U+1F680 (🚀) to U+1F6C0 (🛀)
                        uint32_t emoji_start = 0x1F680;
                        uint32_t emoji_end = 0x1F6C0;
                        uint32_t emoji_code = emoji_start + (esp_random() % (emoji_end - emoji_start + 1));
                        char emoji[5] = {0};
                        // Encode as UTF-8
                        emoji[0] = 0xF0;
                        emoji[1] = 0x9F;
                        emoji[2] = static_cast<char>(0x80 | ((emoji_code >> 6) & 0x3F));
                        emoji[3] = static_cast<char>(0x80 | (emoji_code & 0x3F));
                        ssid += emoji;
                    } break;
                    case 3: {
                        // Add a random emoji from U+1F440 (👀) to U+1F54B (🕋)
                        uint32_t emoji_start = 0x1F440;
                        uint32_t emoji_end = 0x1F54B;
                        uint32_t emoji_code = emoji_start + (esp_random() % (emoji_end - emoji_start + 1));
                        char emoji[5] = {0};
                        emoji[0] = 0xF0;
                        emoji[1] = 0x9F;
                        emoji[2] = static_cast<char>(0x80 | ((emoji_code >> 6) & 0x3F));
                        emoji[3] = static_cast<char>(0x80 | (emoji_code & 0x3F));
                        ssid += emoji;
                    } break;
                }
            }
            sendBeacon(ssid, 0);  // Use macid 0 for emoji mode
            lastBeaconTime = currentMillis;
        }
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
    } else if (current_mode == 3) {
        display->showMainText("Mode: Emoji Spam");
    } else {
        display->showMainText("Unknown Mode");
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
    if (data.compare(APP_1_PRE_STR "3\r\n") == 0) {
        current_mode = 3;
        SetDisplayDirty();
        return true;
    }
    return false;
}