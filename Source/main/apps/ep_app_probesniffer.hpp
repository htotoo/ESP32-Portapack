#ifndef EP_APP_PROBESNIFFER_HPP
#define EP_APP_PROBESNIFFER_HPP

#include "ep_app.hpp"
#include <esp_wifi.h>
#include <vector>
#include <cstring>

#define EP_MAX_PROBES 64  // distinct (device, requested-SSID) rows kept

// One captured probe request: a device asking whether a given network is near.
// Kept small (40 bytes) so a single I2C read stays under the transfer limit.
// Mirrored on the PortaPack side.
typedef struct __attribute__((packed)) {
    uint8_t mac[6];   // source device MAC (may be randomized on modern phones)
    char ssid[32];    // requested network name; empty = broadcast/wildcard probe
    int8_t rssi;      // dBm
    uint8_t channel;  // channel the probe was heard on
} probe_entry_t;

// Passive probe-request sniffer: promiscuous mode + channel hopping, captures
// the networks nearby devices are actively looking for. Purely passive (no TX).
class EPAppProbeSniffer : public EPApp {
   public:
    EPAppProbeSniffer();
    ~EPAppProbeSniffer() override;

    void Loop(uint32_t currentMillis) override;
    bool OnPPData(uint16_t command, std::vector<uint8_t>& data) override;     // IRQ
    bool OnPPReqData(uint16_t command, std::vector<uint8_t>& data) override;  // IRQ

   private:
    static void rxCb(void* buf, wifi_promiscuous_pkt_type_t type);
    void handlePacket(const wifi_promiscuous_pkt_t* pkt);

    static EPAppProbeSniffer* instance;
    static probe_entry_t probes[EP_MAX_PROBES];
    static volatile uint16_t probeCount;

    volatile bool capturing = true;  // hop + store while true; paused freezes list
    uint16_t reqIdx = 0;
    uint8_t curChannel = 1;
    uint32_t lastHopMs = 0;
    static constexpr uint32_t kHopIntervalMs = 250;  // dwell per channel
};

#endif  // EP_APP_PROBESNIFFER_HPP
