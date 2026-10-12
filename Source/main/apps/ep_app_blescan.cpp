#include "ep_app_blescan.hpp"
#include "ble_common.h"
#include "pp_commands.hpp"
#include "esp_log.h"

extern "C" {
#include "host/ble_hs.h"
#include "host/util/util.h"
}

static const char* TAG = "blescan";

EPAppBleScan* EPAppBleScan::instance = nullptr;
ble_dev_t EPAppBleScan::devs[EP_MAX_BLE] = {};
volatile uint16_t EPAppBleScan::devCount = 0;
volatile bool EPAppBleScan::scanning = false;

// dedup + store one advertisement (NimBLE host-task context)
void EPAppBleScan::addDevice(const uint8_t* addr_le, uint8_t type, int8_t rssi,
                             const char* name, uint8_t name_len, uint16_t company, uint8_t kind) {
    uint8_t mac[6];  // NimBLE gives little-endian; store big-endian for display
    for (int i = 0; i < 6; i++) mac[i] = addr_le[5 - i];

    for (uint16_t i = 0; i < devCount; i++) {
        if (memcmp(devs[i].addr, mac, 6) == 0) {
            devs[i].rssi = rssi;
            if (name_len > 0 && devs[i].name[0] == 0) {
                uint8_t n = name_len > 23 ? 23 : name_len;
                memcpy(devs[i].name, name, n);
                devs[i].name[n] = 0;
            }
            if (company && devs[i].company == 0) devs[i].company = company;
            if (kind && devs[i].kind == 0) devs[i].kind = kind;
            return;
        }
    }
    if (devCount >= EP_MAX_BLE) return;
    ble_dev_t& d = devs[devCount];
    memcpy(d.addr, mac, 6);
    d.addr_type = type;
    d.rssi = rssi;
    memset(d.name, 0, sizeof(d.name));
    if (name_len > 0) {
        uint8_t n = name_len > 23 ? 23 : name_len;
        memcpy(d.name, name, n);
        d.name[n] = 0;
    }
    d.company = company;
    d.kind = kind;
    devCount = devCount + 1;
}

// Classify trackers / pairing beacons from the advertisement fields.
static uint8_t classify(const struct ble_hs_adv_fields* f) {
    if (f->mfg_data && f->mfg_data_len >= 3 &&
        f->mfg_data[0] == 0x4C && f->mfg_data[1] == 0x00 && f->mfg_data[2] == 0x12)
        return BLE_KIND_AIRTAG;  // Apple FindMy (AirTag / separated FindMy device)
    for (int i = 0; i < f->num_uuids16; i++) {
        uint16_t u = f->uuids16[i].value;
        if (u == 0xFEED || u == 0xFEEC) return BLE_KIND_TILE;
        if (u == 0xFD5A) return BLE_KIND_SMARTTAG;
        if (u == 0xFE2C) return BLE_KIND_FASTPAIR;
    }
    if (f->svc_data_uuid16 && f->svc_data_uuid16_len >= 2) {
        uint16_t u = f->svc_data_uuid16[0] | (f->svc_data_uuid16[1] << 8);
        if (u == 0xFEED || u == 0xFEEC) return BLE_KIND_TILE;
        if (u == 0xFD5A) return BLE_KIND_SMARTTAG;
        if (u == 0xFE2C || u == 0xFEAA) return BLE_KIND_FASTPAIR;
    }
    if (f->mfg_data && f->mfg_data_len >= 2 && f->mfg_data[0] == 0x06 && f->mfg_data[1] == 0x00)
        return BLE_KIND_MSSWIFT;  // Microsoft Swift Pair
    return BLE_KIND_GENERIC;
}

static int gap_event_cb(struct ble_gap_event* event, void* arg) {
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC && EPAppBleScan::instance) {
        struct ble_hs_adv_fields fields;
        memset(&fields, 0, sizeof(fields));
        ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        uint16_t company = 0;
        if (fields.mfg_data && fields.mfg_data_len >= 2)
            company = fields.mfg_data[0] | (fields.mfg_data[1] << 8);
        EPAppBleScan::instance->addDevice(
            event->disc.addr.val, event->disc.addr.type, event->disc.rssi,
            (const char*)fields.name, fields.name_len, company, classify(&fields));
    }
    return 0;
}

static void start_scan() {
    struct ble_gap_disc_params dp;
    memset(&dp, 0, sizeof(dp));
    dp.passive = 1;             // just listen (lighter, less RF)
    dp.filter_duplicates = 0;   // we dedup ourselves and want RSSI updates
    dp.itvl = 0;
    dp.window = 0;
    if (ble_gap_disc(ble_common_own_addr_type(), BLE_HS_FOREVER, &dp, gap_event_cb, NULL) == 0)
        EPAppBleScan::scanning = true;
}

EPAppBleScan::EPAppBleScan() {
    instance = this;
    devCount = 0;
    ble_common_ensure_init();  // scan starts from Loop() once the host is synced
    ESP_LOGI(TAG, "BLE scan app created");
}

EPAppBleScan::~EPAppBleScan() {
    if (scanning) {
        ble_gap_disc_cancel();
        scanning = false;
    }
    instance = nullptr;  // leave NimBLE resident for next time
}

void EPAppBleScan::Loop(uint32_t currentMillis) {
    (void)currentMillis;
    if (!ble_common_synced()) return;  // wait for the host to come up
    if (!started_) {
        started_ = true;
        scanWant = true;
        scanChangeReq = false;
        start_scan();
        return;
    }
    // apply a pause/resume requested from the I2C ISR (NimBLE calls need task ctx)
    if (scanChangeReq) {
        scanChangeReq = false;
        if (scanWant && !scanning) {
            start_scan();
        } else if (!scanWant && scanning) {
            ble_gap_disc_cancel();
            scanning = false;
        }
    }
}

// IRQ CALLBACK
bool EPAppBleScan::OnPPData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_BLESCAN_GET_ENTRY) {
        if (data.size() >= 2) reqIdx = *reinterpret_cast<uint16_t*>(data.data());
        return true;
    } else if (command == PPCMD_BLESCAN_SET_AUTO) {
        if (data.size() >= 1) {
            scanWant = (data[0] != 0);  // Loop() does the NimBLE call (not the ISR)
            scanChangeReq = true;
        }
        return true;
    } else if (command == PPCMD_BLESCAN_CLEAR) {
        devCount = 0;
        return true;
    }
    return false;
}

// IRQ CALLBACK (reads fixed buffers only)
bool EPAppBleScan::OnPPReqData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_APPMGR_APPCMD) {
        data.resize(6);
        uint16_t* p = reinterpret_cast<uint16_t*>(data.data());
        p[0] = devCount;
        p[1] = scanning ? 1 : 0;
        p[2] = 0;
        return true;
    } else if (command == PPCMD_BLESCAN_GET_ENTRY) {
        data.resize(sizeof(ble_dev_t));
        memset(data.data(), 0, sizeof(ble_dev_t));
        if (reqIdx < devCount) memcpy(data.data(), &devs[reqIdx], sizeof(ble_dev_t));
        return true;
    }
    return false;
}
