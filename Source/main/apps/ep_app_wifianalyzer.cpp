#include "ep_app_wifianalyzer.hpp"
#include "pp_commands.hpp"
#include "esp_log.h"
#include <cstdlib>

static const char* TAG = "wifianalyzer";

EPAppWifiAnalyzer* EPAppWifiAnalyzer::instance = nullptr;
wifi_ap_entry_t EPAppWifiAnalyzer::aps[EP_MAX_APS] = {};
volatile uint16_t EPAppWifiAnalyzer::apCount = 0;
volatile bool EPAppWifiAnalyzer::scanning = false;

EPAppWifiAnalyzer::EPAppWifiAnalyzer() {
    instance = this;
    ESP_LOGI(TAG, "WiFi analyzer app created");
}

EPAppWifiAnalyzer::~EPAppWifiAnalyzer() {
    // stop any scan in progress so we don't leave the driver busy
    esp_wifi_scan_stop();
    instance = nullptr;
}

// Runs in the main task context (safe to block). The module boots in APSTA,
// so a STA interface is always available for scanning.
void EPAppWifiAnalyzer::performScan() {
    scanning = true;

    wifi_scan_config_t cfg = {};
    cfg.ssid = nullptr;
    cfg.bssid = nullptr;
    cfg.channel = 0;         // all channels
    cfg.show_hidden = true;  // include hidden SSIDs (shown as <hidden>)
    cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    cfg.scan_time.active.min = 60;
    cfg.scan_time.active.max = 120;

    if (esp_wifi_scan_start(&cfg, true) != ESP_OK) {
        scanning = false;
        return;
    }

    uint16_t n = EP_MAX_APS;
    wifi_ap_record_t* recs = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * EP_MAX_APS);
    if (recs == nullptr) {
        esp_wifi_clear_ap_list();
        scanning = false;
        return;
    }
    if (esp_wifi_scan_get_ap_records(&n, recs) != ESP_OK) n = 0;
    if (n > EP_MAX_APS) n = EP_MAX_APS;

    for (uint16_t i = 0; i < n; i++) {
        wifi_ap_entry_t& e = aps[i];
        memcpy(e.bssid, recs[i].bssid, 6);
        memcpy(e.ssid, recs[i].ssid, 32);
        e.ssid[31] = 0;
        e.channel = recs[i].primary;
        e.rssi = (int8_t)recs[i].rssi;
        // second: 0=none/20MHz, 1=above, 2=below
        e.second = (recs[i].second == WIFI_SECOND_CHAN_ABOVE) ? 1 : (recs[i].second == WIFI_SECOND_CHAN_BELOW) ? 2 : 0;
        e.authmode = (uint8_t)recs[i].authmode;
    }
    apCount = n;
    free(recs);
    ESP_LOGI(TAG, "scan done, %u APs", (unsigned)n);
    scanning = false;
}

void EPAppWifiAnalyzer::Loop(uint32_t currentMillis) {
    if (scanning) return;
    bool due = rescanReq || (autoScan && (currentMillis - lastScanMs > kRescanIntervalMs));
    if (due) {
        rescanReq = false;
        lastScanMs = currentMillis;
        performScan();
    }
}

// IRQ CALLBACK
bool EPAppWifiAnalyzer::OnPPData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_WIFIANALYZER_GET_ENTRY) {
        if (data.size() >= 2) reqIdx = *reinterpret_cast<uint16_t*>(data.data());
        return true;
    } else if (command == PPCMD_WIFIANALYZER_RESCAN) {
        rescanReq = true;
        return true;
    } else if (command == PPCMD_WIFIANALYZER_SET_AUTO) {
        if (data.size() >= 1) autoScan = (data[0] != 0);
        return true;
    }
    return false;
}

// IRQ CALLBACK (reads fixed buffers only)
bool EPAppWifiAnalyzer::OnPPReqData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_APPMGR_APPCMD) {
        data.resize(6);
        uint16_t* p = reinterpret_cast<uint16_t*>(data.data());
        p[0] = apCount;
        p[1] = scanning ? 1 : 0;
        p[2] = 0;
        return true;
    } else if (command == PPCMD_WIFIANALYZER_GET_ENTRY) {
        data.resize(sizeof(wifi_ap_entry_t));
        memset(data.data(), 0, sizeof(wifi_ap_entry_t));
        if (reqIdx < apCount) memcpy(data.data(), &aps[reqIdx], sizeof(wifi_ap_entry_t));
        return true;
    }
    return false;
}
