#include "ble_common.h"
#include "esp_log.h"

extern "C" {
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
}

static const char* TAG = "ble_common";
static bool s_inited = false;
static volatile bool s_synced = false;
static uint8_t s_own_addr_type = 0;

static void on_sync(void) {
    ble_hs_util_ensure_addr(0);
    ble_hs_id_infer_auto(0, &s_own_addr_type);
    s_synced = true;
    ESP_LOGI(TAG, "NimBLE synced");
}

static void host_task(void* param) {
    (void)param;
    nimble_port_run();  // returns only on nimble_port_stop
    nimble_port_freertos_deinit();
}

extern "C" bool ble_common_ensure_init(void) {
    if (s_inited) return true;
    if (nimble_port_init() != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed");
        return false;
    }
    ble_hs_cfg.sync_cb = on_sync;
    ble_svc_gap_init();
    nimble_port_freertos_init(host_task);
    s_inited = true;
    return true;
}

extern "C" bool ble_common_synced(void) { return s_synced; }
extern "C" uint8_t ble_common_own_addr_type(void) { return s_own_addr_type; }
