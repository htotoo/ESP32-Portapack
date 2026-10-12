#ifndef EP_APP_BLESCAN_HPP
#define EP_APP_BLESCAN_HPP

#include "ep_app.hpp"
#include <vector>
#include <cstring>

#define EP_MAX_BLE 48

// device kind (tracker classification)
#define BLE_KIND_GENERIC 0
#define BLE_KIND_AIRTAG 1    // Apple FindMy / AirTag
#define BLE_KIND_TILE 2
#define BLE_KIND_SMARTTAG 3  // Samsung
#define BLE_KIND_FASTPAIR 4  // Google Fast Pair
#define BLE_KIND_MSSWIFT 5   // Microsoft Swift Pair

// One scanned BLE device (35 bytes) — fits a single I2C read. Mirrored on PP.
typedef struct __attribute__((packed)) {
    uint8_t addr[6];     // big-endian display order
    uint8_t addr_type;   // 0=public, 1=random (privacy), ...
    int8_t rssi;
    char name[24];       // advertised name (may be empty)
    uint16_t company;    // manufacturer company id (0 = none)
    uint8_t kind;        // BLE_KIND_*
} ble_dev_t;

// Passive BLE advertisement scanner using NimBLE (already compiled in; WiFi/BLE
// coexistence is enabled). Lists nearby Bluetooth LE devices.
class EPAppBleScan : public EPApp {
   public:
    EPAppBleScan();
    ~EPAppBleScan() override;

    void Loop(uint32_t currentMillis) override;
    bool OnPPData(uint16_t command, std::vector<uint8_t>& data) override;     // IRQ
    bool OnPPReqData(uint16_t command, std::vector<uint8_t>& data) override;  // IRQ

    // called from the NimBLE host task
    void addDevice(const uint8_t* addr_le, uint8_t type, int8_t rssi,
                   const char* name, uint8_t name_len, uint16_t company, uint8_t kind);

    static EPAppBleScan* instance;
    static volatile bool scanning;

   private:
    static ble_dev_t devs[EP_MAX_BLE];
    static volatile uint16_t devCount;
    uint16_t reqIdx = 0;
    // NimBLE calls must run in task context, never from the I2C ISR (OnPPData).
    // The ISR only sets these; Loop() applies them.
    volatile bool scanChangeReq = false;
    volatile bool scanWant = true;
    bool started_ = false;
};

#endif  // EP_APP_BLESCAN_HPP
