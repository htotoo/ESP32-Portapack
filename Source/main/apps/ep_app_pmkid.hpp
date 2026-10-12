#ifndef EP_APP_PMKID_HPP
#define EP_APP_PMKID_HPP

#include "ep_app.hpp"
#include <esp_wifi.h>
#include <vector>
#include <cstring>

#define EP_MAX_HS 12       // captured PMKIDs/handshakes kept
#define EP_HC_LINE_MAX 512  // max length of a hashcat 22000 line
#define EP_MAX_PENDING 8    // half-open M1s waiting for their M2
#define EP_MAX_BEACONS 48   // BSSID->SSID map size

// Compact display record for one capture (fits a single I2C read). The full
// hashcat 22000 line is fetched separately in chunks. Mirrored on the PP side.
typedef struct __attribute__((packed)) {
    uint8_t ap[6];
    uint8_t sta[6];
    char ssid[32];
    uint8_t type;     // 1 = PMKID, 2 = EAPOL 4-way handshake
    uint8_t channel;
} hs_disp_t;

// Passive WPA2 key-material capture: PMKID (type 01) from EAPOL M1 when present,
// and full 4-way handshakes (type 02, M1+M2) which work on all WPA2 APs. Builds
// hashcat-22000 lines the PortaPack saves to SD; cracking happens offline on a PC.
class EPAppPmkid : public EPApp {
   public:
    EPAppPmkid();
    ~EPAppPmkid() override;

    void Loop(uint32_t currentMillis) override;
    bool OnPPData(uint16_t command, std::vector<uint8_t>& data) override;     // IRQ
    bool OnPPReqData(uint16_t command, std::vector<uint8_t>& data) override;  // IRQ

   private:
    static void rxCb(void* buf, wifi_promiscuous_pkt_type_t type);
    void handleMgmt(const wifi_promiscuous_pkt_t* pkt);
    void handleData(const wifi_promiscuous_pkt_t* pkt);
    void noteBeacon(const uint8_t* bssid, const char* ssid, uint8_t channel);
    void sendDeauthRound();
    void resolveSsid(const uint8_t* bssid, char* out);  // out[32]
    bool havePair(const uint8_t* ap, const uint8_t* sta, uint8_t type);
    void addPmkid(const uint8_t* ap, const uint8_t* sta, const uint8_t* pmkid, uint8_t ch);
    void addHandshake(const uint8_t* ap, const uint8_t* sta, const uint8_t* anonce,
                      const uint8_t* mic, const uint8_t* eapol, int eapolLen, uint8_t ch);

    static EPAppPmkid* instance;
    static hs_disp_t disp[EP_MAX_HS];
    static char hcLine[EP_MAX_HS][EP_HC_LINE_MAX];
    static uint16_t hcLen[EP_MAX_HS];
    static volatile uint16_t hsCount;

    static volatile uint16_t eapolSeen;
    static volatile uint16_t beaconCount;
    // diagnostics for the handshake pairing
    static volatile uint16_t m1Seen;
    static volatile uint16_t m2Seen;
    static volatile uint16_t pairedSeen;

    struct beacon_t {
        uint8_t bssid[6];
        char ssid[32];
        uint8_t channel;
    };
    static beacon_t beacons[EP_MAX_BEACONS];

    struct pending_t {
        uint8_t ap[6];
        uint8_t sta[6];
        uint8_t anonce[32];
        uint8_t replay[8];
        bool used;
    };
    static pending_t pend[EP_MAX_PENDING];
    static uint8_t pendNext;

    volatile bool capturing = true;
    volatile uint8_t lockChannel = 0;  // 0 = hop; 1-13 = stay
    volatile uint8_t deauthRounds = 0;  // >0 = send a deauth burst from Loop()
    static volatile uint16_t deauthSent;
    uint16_t reqIdx = 0;               // GET_ENTRY (display record)
    uint16_t reqLineIdx = 0;           // line whose chunk is requested
    uint16_t reqLineOff = 0;
    uint8_t curChannel = 1;
    uint32_t lastHopMs = 0;
    static constexpr uint32_t kHopIntervalMs = 300;
};

#endif  // EP_APP_PMKID_HPP
