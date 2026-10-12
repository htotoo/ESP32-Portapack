#ifndef BLE_COMMON_H
#define BLE_COMMON_H

#include <stdbool.h>
#include <stdint.h>

// Shared NimBLE bring-up for all BLE apps. nimble_port_init() must run only
// once, so both the scanner and the spammer route through here.
#ifdef __cplusplus
extern "C" {
#endif

bool ble_common_ensure_init(void);  // idempotent; true once NimBLE is up
bool ble_common_synced(void);       // host+controller ready
uint8_t ble_common_own_addr_type(void);

#ifdef __cplusplus
}
#endif

#endif  // BLE_COMMON_H
