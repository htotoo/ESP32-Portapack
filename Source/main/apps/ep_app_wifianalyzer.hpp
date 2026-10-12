#ifndef EP_APP_WIFIANALYZER_HPP
#define EP_APP_WIFIANALYZER_HPP

#include "ep_app.hpp"
#include <esp_wifi.h>
#include <vector>
#include <cstring>

#define EP_MAX_APS 48  // max networks kept per scan

// One scanned access point. Kept small (42 bytes) so a single I2C read stays
// well under the transfer limit. Mirrored on the PortaPack side.
typedef struct __attribute__((packed)) {
    uint8_t bssid[6];
    char ssid[32];    // null-terminated
    uint8_t channel;  // primary channel 1..14 (2.4 GHz)
    int8_t rssi;      // dBm
    uint8_t second;   // HT secondary channel: 0=none(20MHz), 1=above, 2=below
    uint8_t authmode; // wifi_auth_mode_t
} wifi_ap_entry_t;

// Passive 2.4 GHz WiFi analyzer: periodic esp_wifi_scan, exposes the AP list
// (SSID / BSSID / channel / RSSI) over I2C so the PortaPack shows the exact
// broadcast frequency (for the jammer). No raw-frame hacks; scan only.
class EPAppWifiAnalyzer : public EPApp {
   public:
    EPAppWifiAnalyzer();
    ~EPAppWifiAnalyzer() override;

    void Loop(uint32_t currentMillis) override;
    bool OnPPData(uint16_t command, std::vector<uint8_t>& data) override;     // IRQ
    bool OnPPReqData(uint16_t command, std::vector<uint8_t>& data) override;  // IRQ

   private:
    void performScan();

    static EPAppWifiAnalyzer* instance;
    static wifi_ap_entry_t aps[EP_MAX_APS];
    static volatile uint16_t apCount;
    static volatile bool scanning;

    volatile bool rescanReq = true;  // scan once as soon as the app starts
    volatile bool autoScan = true;   // periodic rescan on/off (toggled from PP)
    uint16_t reqIdx = 0;             // index requested by the last GET_ENTRY
    uint32_t lastScanMs = 0;
    static constexpr uint32_t kRescanIntervalMs = 8000;  // auto-refresh cadence
};

#endif  // EP_APP_WIFIANALYZER_HPP
