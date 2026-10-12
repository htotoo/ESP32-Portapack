#include "ep_app_probesniffer.hpp"
#include "pp_commands.hpp"
#include "esp_log.h"

static const char* TAG = "probesniffer";

EPAppProbeSniffer* EPAppProbeSniffer::instance = nullptr;
probe_entry_t EPAppProbeSniffer::probes[EP_MAX_PROBES] = {};
volatile uint16_t EPAppProbeSniffer::probeCount = 0;

EPAppProbeSniffer::EPAppProbeSniffer() {
    instance = this;
    probeCount = 0;
    // The module boots in APSTA (started); enable promiscuous capture of mgmt
    // frames and route them to our static callback.
    wifi_promiscuous_filter_t filter = {};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&EPAppProbeSniffer::rxCb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
    ESP_LOGI(TAG, "Probe sniffer app created");
}

EPAppProbeSniffer::~EPAppProbeSniffer() {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    instance = nullptr;
}

// WiFi-task context. Parse a probe request and store one (device, SSID) row.
void EPAppProbeSniffer::handlePacket(const wifi_promiscuous_pkt_t* pkt) {
    if (!capturing) return;
    const uint8_t* p = pkt->payload;
    // Frame Control byte 0 == 0x40 => management / subtype probe request.
    if (p[0] != 0x40) return;

    const uint8_t* mac = p + 10;  // addr2 = source (the device)

    // Tagged params start at offset 24; first tag should be SSID (id 0).
    char ssid[32];
    memset(ssid, 0, sizeof(ssid));
    if (p[24] == 0x00) {
        uint8_t len = p[25];
        if (len > 31) len = 31;
        memcpy(ssid, p + 26, len);
        ssid[len] = 0;
    }

    int8_t rssi = pkt->rx_ctrl.rssi;
    uint8_t ch = pkt->rx_ctrl.channel;

    // Dedup on (MAC, SSID): update if seen, else append.
    for (uint16_t i = 0; i < probeCount; i++) {
        if (memcmp(probes[i].mac, mac, 6) == 0 && strncmp(probes[i].ssid, ssid, 32) == 0) {
            probes[i].rssi = rssi;
            probes[i].channel = ch;
            return;
        }
    }
    if (probeCount >= EP_MAX_PROBES) return;
    probe_entry_t& e = probes[probeCount];
    memcpy(e.mac, mac, 6);
    memcpy(e.ssid, ssid, 32);
    e.rssi = rssi;
    e.channel = ch;
    probeCount = probeCount + 1;  // publish last, so ISR sees complete entries
}

void EPAppProbeSniffer::rxCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT || instance == nullptr) return;
    instance->handlePacket((const wifi_promiscuous_pkt_t*)buf);
}

void EPAppProbeSniffer::Loop(uint32_t currentMillis) {
    if (!capturing) return;  // frozen: stay on the current channel
    if (currentMillis - lastHopMs > kHopIntervalMs) {
        lastHopMs = currentMillis;
        curChannel++;
        if (curChannel > 13) curChannel = 1;
        esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
    }
}

// IRQ CALLBACK
bool EPAppProbeSniffer::OnPPData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_PROBESNIFFER_GET_ENTRY) {
        if (data.size() >= 2) reqIdx = *reinterpret_cast<uint16_t*>(data.data());
        return true;
    } else if (command == PPCMD_PROBESNIFFER_SET_AUTO) {
        if (data.size() >= 1) capturing = (data[0] != 0);
        return true;
    } else if (command == PPCMD_PROBESNIFFER_CLEAR) {
        probeCount = 0;
        return true;
    }
    return false;
}

// IRQ CALLBACK (reads fixed buffers only)
bool EPAppProbeSniffer::OnPPReqData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_APPMGR_APPCMD) {
        data.resize(6);
        uint16_t* p = reinterpret_cast<uint16_t*>(data.data());
        p[0] = probeCount;
        p[1] = capturing ? 1 : 0;
        p[2] = 0;
        return true;
    } else if (command == PPCMD_PROBESNIFFER_GET_ENTRY) {
        data.resize(sizeof(probe_entry_t));
        memset(data.data(), 0, sizeof(probe_entry_t));
        if (reqIdx < probeCount) memcpy(data.data(), &probes[reqIdx], sizeof(probe_entry_t));
        return true;
    }
    return false;
}
