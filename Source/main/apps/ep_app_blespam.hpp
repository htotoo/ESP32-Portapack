#ifndef EP_APP_BLESPAM_HPP
#define EP_APP_BLESPAM_HPP

#include "ep_app.hpp"
#include <vector>
#include <cstring>

// BLE advertisement spam: cycles fake pairing beacons (Apple popup, Google
// Fast Pair, Microsoft Swift Pair) with a random address, to spawn pairing
// popups on nearby devices. Awareness / nuisance demo — authorized use only.
class EPAppBleSpam : public EPApp {
   public:
    EPAppBleSpam();
    ~EPAppBleSpam() override;

    void Loop(uint32_t currentMillis) override;
    bool OnPPData(uint16_t command, std::vector<uint8_t>& data) override;     // IRQ
    bool OnPPReqData(uint16_t command, std::vector<uint8_t>& data) override;  // IRQ

   private:
    volatile uint8_t mode = 0;    // 0=Apple,1=FastPair,2=SwiftPair,3=all,4=custom(SD)
    volatile bool active = false;
    static volatile uint32_t sentCount;
    uint32_t lastMs = 0;

    // custom payloads uploaded from the PortaPack SD card (BLUETOOTH/BLESPAM)
    static constexpr int MAX_CUSTOM = 24;
    static uint8_t customData[MAX_CUSTOM][31];
    static uint8_t customLen[MAX_CUSTOM];
    static volatile uint8_t customCount;
};

#endif  // EP_APP_BLESPAM_HPP
