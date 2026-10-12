#ifndef EP_APP_DRONE_HPP
#define EP_APP_DRONE_HPP

#include "ep_app.hpp"
#include <esp_wifi.h>
#include <vector>
#include <cstring>

#define EP_MAX_DRONES 24

#define DRONE_SRC_BLE_RID 1   // Open Drone ID over Bluetooth
#define DRONE_SRC_WIFI_RID 2  // Open Drone ID over WiFi beacon
#define DRONE_SRC_WIFI_OUI 3  // known drone-maker WiFi (OUI / SSID)

// One detected drone (44 bytes) — fits a single I2C read. Mirrored on PP.
typedef struct __attribute__((packed)) {
    uint8_t source;    // DRONE_SRC_*
    char id[20];       // UAS ID / serial (Basic ID), or SSID for OUI-only
    int32_t lat;       // drone latitude, deg * 1e7 (0 = unknown)
    int32_t lon;       // drone longitude
    int32_t op_lat;    // operator latitude
    int32_t op_lon;    // operator longitude
    int8_t rssi;
    uint8_t mac[6];    // BSSID or BLE address
} drone_t;

// Drone detector: WiFi promiscuous (beacon OUI/SSID + WiFi Remote-ID vendor IE)
// + BLE scan (Open Drone ID service 0xFFFA). Parses ASTM F3411 Remote ID.
class EPAppDrone : public EPApp {
   public:
    EPAppDrone();
    ~EPAppDrone() override;

    void Loop(uint32_t currentMillis) override;
    bool OnPPData(uint16_t command, std::vector<uint8_t>& data) override;     // IRQ
    bool OnPPReqData(uint16_t command, std::vector<uint8_t>& data) override;  // IRQ

    // parse one ASTM ODID message into a drone record (public for the callbacks)
    static void parseOdid(const uint8_t* msg, int len, drone_t* d);
    void addOrUpdate(const drone_t& in);

    static EPAppDrone* instance;
    static volatile bool started;

   private:
    static void wifiRxCb(void* buf, wifi_promiscuous_pkt_type_t type);
    void handleBeacon(const wifi_promiscuous_pkt_t* pkt);

    static drone_t drones[EP_MAX_DRONES];
    static volatile uint16_t droneCount;

    uint16_t reqIdx = 0;
    uint8_t curChannel = 1;
    uint32_t lastHopMs = 0;
};

#endif  // EP_APP_DRONE_HPP
