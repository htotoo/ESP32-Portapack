#pragma once
#include <esp_wifi.h>
// Bypass 802.11 RAW frames sanity check (allows spoofed source MACs for the
// WiFi-spam app). Always return 1 ("valid") so that NORMAL AP frames — e.g.
// the SoftAP's DHCP OFFER/ACK to clients — are not dropped. The old
// `else return 0` rejected every normal frame, which broke client DHCP on the
// module's own AP (clients ended up with 169.254.x APIPA).
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    (void)arg;
    (void)arg2;
    (void)arg3;
    return 1;
}  // -Wl,-zmuldefs
