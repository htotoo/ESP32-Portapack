#include "ep_app_drone.hpp"
#include "ble_common.h"
#include "pp_commands.hpp"
#include "esp_log.h"

extern "C" {
#include "host/ble_hs.h"
#include "host/util/util.h"
}

static const char* TAG = "drone";

EPAppDrone* EPAppDrone::instance = nullptr;
drone_t EPAppDrone::drones[EP_MAX_DRONES] = {};
volatile uint16_t EPAppDrone::droneCount = 0;
volatile bool EPAppDrone::started = false;
static bool s_ble_started = false;

static int32_t le32(const uint8_t* p) {
    return (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24));
}

// ---- known drone-maker WiFi OUIs (first 3 MAC bytes) ----
static bool drone_oui(const uint8_t* m) {
    static const uint8_t ouis[][3] = {
        {0x60, 0x60, 0x1F}, {0x34, 0xD2, 0x62}, {0x48, 0x1C, 0xB9}, {0xE4, 0x7A, 0x2C},
        {0x58, 0xB8, 0x58}, {0x0C, 0x11, 0x05}, {0x1C, 0x5B, 0x0E}, {0x2C, 0x4D, 0x79},  // DJI
        {0x90, 0x3A, 0xE6}, {0xA0, 0x14, 0x3D}, {0x00, 0x12, 0x1C}, {0x00, 0x26, 0x7E},  // Parrot
        {0x00, 0x0C, 0x43}, {0xB4, 0x3A, 0x28}};                                          // Autel/others
    for (unsigned i = 0; i < sizeof(ouis) / 3; i++)
        if (m[0] == ouis[i][0] && m[1] == ouis[i][1] && m[2] == ouis[i][2]) return true;
    return false;
}
static bool drone_ssid(const char* s) {
    static const char* kw[] = {"DJI", "Mavic", "Tello", "Parrot", "ANAFI", "Autel",
                               "EVO", "Skydio", "FIMI", "Yuneec", "Drone", "UAV"};
    for (unsigned i = 0; i < sizeof(kw) / sizeof(kw[0]); i++)
        if (strstr(s, kw[i])) return true;
    return false;
}

// Parse one 25-byte ASTM F3411 Open Drone ID message.
void EPAppDrone::parseOdid(const uint8_t* msg, int len, drone_t* d) {
    if (len < 1) return;
    uint8_t type = msg[0] >> 4;
    if (type == 0x0 && len >= 22) {  // Basic ID -> UAS ID
        memcpy(d->id, msg + 2, 20);
        d->id[19] = 0;
    } else if (type == 0x1 && len >= 13) {  // Location/Vector
        d->lat = le32(msg + 5);
        d->lon = le32(msg + 9);
    } else if (type == 0x4 && len >= 10) {  // System -> operator location
        d->op_lat = le32(msg + 2);
        d->op_lon = le32(msg + 6);
    } else if (type == 0xF && len >= 3) {  // Message pack
        int msize = msg[1] ? msg[1] : 25;
        int n = msg[2];
        for (int i = 0; i < n; i++) {
            int off = 3 + i * msize;
            if (off + msize <= len) parseOdid(msg + off, msize, d);
        }
    }
}

void EPAppDrone::addOrUpdate(const drone_t& in) {
    for (uint16_t i = 0; i < droneCount; i++) {
        if (memcmp(drones[i].mac, in.mac, 6) == 0) {
            drones[i].rssi = in.rssi;
            if (in.lat || in.lon) { drones[i].lat = in.lat; drones[i].lon = in.lon; }
            if (in.op_lat || in.op_lon) { drones[i].op_lat = in.op_lat; drones[i].op_lon = in.op_lon; }
            if (in.id[0] && drones[i].id[0] == 0) memcpy(drones[i].id, in.id, 20);
            if (in.source < drones[i].source) drones[i].source = in.source;  // RID beats OUI
            return;
        }
    }
    if (droneCount >= EP_MAX_DRONES) return;
    drones[droneCount] = in;
    droneCount = droneCount + 1;
    ESP_LOGI(TAG, "drone detected (%u)", (unsigned)droneCount);
}

// ---- BLE: Open Drone ID over Bluetooth (service data UUID 0xFFFA) ----
static int drone_ble_cb(struct ble_gap_event* event, void* arg) {
    (void)arg;
    if (event->type != BLE_GAP_EVENT_DISC || !EPAppDrone::instance) return 0;
    struct ble_hs_adv_fields f;
    memset(&f, 0, sizeof(f));
    ble_hs_adv_parse_fields(&f, event->disc.data, event->disc.length_data);
    if (!f.svc_data_uuid16 || f.svc_data_uuid16_len < 6) return 0;
    // [uuid16 FA FF][app 0x0D][counter][message...]
    if (f.svc_data_uuid16[0] != 0xFA || f.svc_data_uuid16[1] != 0xFF) return 0;
    if (f.svc_data_uuid16[2] != 0x0D) return 0;
    const uint8_t* msg = f.svc_data_uuid16 + 4;
    int mlen = f.svc_data_uuid16_len - 4;
    drone_t d;
    memset(&d, 0, sizeof(d));
    d.source = DRONE_SRC_BLE_RID;
    d.rssi = event->disc.rssi;
    for (int i = 0; i < 6; i++) d.mac[i] = event->disc.addr.val[5 - i];
    EPAppDrone::parseOdid(msg, mlen, &d);
    EPAppDrone::instance->addOrUpdate(d);
    return 0;
}

// ---- WiFi: beacon OUI/SSID + Open Drone ID vendor IE (OUI FA 0B BC) ----
void EPAppDrone::handleBeacon(const wifi_promiscuous_pkt_t* pkt) {
    const uint8_t* p = pkt->payload;
    if (p[0] != 0x80) return;  // beacon
    int len = pkt->rx_ctrl.sig_len;
    if (len < 38) return;
    const uint8_t* bssid = p + 16;

    char ssid[33] = {0};
    bool isRid = false;
    drone_t d;
    memset(&d, 0, sizeof(d));

    const uint8_t* tags = p + 36;
    int remain = len - 36 - 4;
    while (remain >= 2) {
        uint8_t tag = tags[0], tlen = tags[1];
        if (2 + tlen > remain) break;
        if (tag == 0 && tlen <= 32) {  // SSID
            memcpy(ssid, tags + 2, tlen);
            ssid[tlen] = 0;
        } else if (tag == 0xDD && tlen >= 5 &&
                   tags[2] == 0xFA && tags[3] == 0x0B && tags[4] == 0xBC) {  // ASTM Remote ID
            // vendor IE: OUI(3) type(1=0x0D) counter(1) then ODID message pack
            const uint8_t* msg = tags + 7;
            int mlen = tlen - 5;
            parseOdid(msg, mlen, &d);
            isRid = true;
        }
        tags += 2 + tlen;
        remain -= 2 + tlen;
    }

    if (isRid) {
        d.source = DRONE_SRC_WIFI_RID;
        d.rssi = pkt->rx_ctrl.rssi;
        memcpy(d.mac, bssid, 6);
        if (d.id[0] == 0) strncpy(d.id, ssid, 19);
        addOrUpdate(d);
    } else if (drone_oui(bssid) || (ssid[0] && drone_ssid(ssid))) {
        drone_t o;
        memset(&o, 0, sizeof(o));
        o.source = DRONE_SRC_WIFI_OUI;
        o.rssi = pkt->rx_ctrl.rssi;
        memcpy(o.mac, bssid, 6);
        strncpy(o.id, ssid[0] ? ssid : "drone", 19);
        addOrUpdate(o);
    }
}

void EPAppDrone::wifiRxCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT || !instance) return;
    instance->handleBeacon((const wifi_promiscuous_pkt_t*)buf);
}

EPAppDrone::EPAppDrone() {
    instance = this;
    droneCount = 0;
    started = false;
    s_ble_started = false;
    // WiFi promiscuous for beacons
    wifi_promiscuous_filter_t filter = {};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&EPAppDrone::wifiRxCb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
    // BLE: shared init; scan starts from Loop once synced
    ble_common_ensure_init();
    ESP_LOGI(TAG, "Drone detector created");
}

EPAppDrone::~EPAppDrone() {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    if (s_ble_started) ble_gap_disc_cancel();
    instance = nullptr;
}

void EPAppDrone::Loop(uint32_t currentMillis) {
    // start BLE scan once the host is synced
    if (!s_ble_started && ble_common_synced()) {
        struct ble_gap_disc_params dp;
        memset(&dp, 0, sizeof(dp));
        dp.passive = 1;
        dp.filter_duplicates = 0;
        if (ble_gap_disc(ble_common_own_addr_type(), BLE_HS_FOREVER, &dp, drone_ble_cb, NULL) == 0)
            s_ble_started = true;
    }
    // hop WiFi channels for beacons
    if (currentMillis - lastHopMs > 300) {
        lastHopMs = currentMillis;
        curChannel++;
        if (curChannel > 13) curChannel = 1;
        esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
    }
}

// IRQ CALLBACK
bool EPAppDrone::OnPPData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_DRONE_GET_ENTRY) {
        if (data.size() >= 2) reqIdx = *reinterpret_cast<uint16_t*>(data.data());
        return true;
    } else if (command == PPCMD_DRONE_CLEAR) {
        droneCount = 0;
        return true;
    }
    return false;
}

// IRQ CALLBACK
bool EPAppDrone::OnPPReqData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_APPMGR_APPCMD) {
        data.resize(6);
        uint16_t* p = reinterpret_cast<uint16_t*>(data.data());
        p[0] = droneCount;
        p[1] = 1;
        p[2] = 0;
        return true;
    } else if (command == PPCMD_DRONE_GET_ENTRY) {
        data.resize(sizeof(drone_t));
        memset(data.data(), 0, sizeof(drone_t));
        if (reqIdx < droneCount) memcpy(data.data(), &drones[reqIdx], sizeof(drone_t));
        return true;
    }
    return false;
}
