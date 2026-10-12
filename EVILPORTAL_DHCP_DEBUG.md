# EvilPortal — open probleem: clients krijgen geen DHCP/data (WIP)

Status bij pauze (build 0.48-wip). De EvilPortal-feature werkt grotendeels
(app onder Utilities, start/stop, clients-lijst, captures, SD-HTML, AP-config),
**behalve**: een client die met de portal-AP verbindt krijgt **geen werkend
IP en geen dataverkeer**.

## Symptomen (bevestigd op iPhone én MacBook)
- Client **associeert** met "Free WiFi" (station join + `WIFI_EVENT_AP_STACONNECTED`
  in de seriële log). Management-frames werken dus.
- **Geen** DHCP-lease bij de client → client valt terug op APIPA **169.254.x**.
- ESP dhcps **reserveert** wél een adres: PortaPack-clientlijst toont 192.168.4.2
  (komt uit `esp_wifi_ap_get_sta_list_with_ip`, de lease-tabel — niet het echte
  client-IP).
- Met **handmatig** IP (192.168.4.50/24, gw 192.168.4.1) is `http://192.168.4.1`
  **niet** bereikbaar → dus **geen enkel dataverkeer** AP↔client, niet alleen DHCP.
- Serieel: na `station join` volgt **geen** `IP_EVENT_AP_STAIPASSIGNED` en geen
  data-logs.

## Wat GEEN oorzaak bleek
- **wifihack.hpp** (raw-frame sanity override): uitgeschakeld in test 0.46 →
  probleem bleef. Weer ingeschakeld (WiFi-spam blijft werken).
- **Eigen AP-only bring-up** vs **firmware APSTA-pad** (`config_wifi_apsta`):
  0.47 gebruikt nu het firmware-eigen APSTA-pad → probleem bleef.
- Expliciete dhcps IP/pool/DNS + expliciete start (0.43) en minimale auto-dhcps
  (0.44): beide → probleem bleef.

## Sterkste openstaande hypothesen (voor volgende sessie)
1. **AP power-save / buffering** richting power-saving clients (iPhones): AP
   levert gebufferde frames niet af. Onderzoek: `esp_wifi_set_ps(WIFI_PS_NONE)`,
   DTIM/beacon-interval, `wifi:Total power save buffer number: 16`.
2. **Netif / "meerdere virtuele devices"** (tip): mogelijk hangt het datapad aan
   een verkeerd/verouderd netif, of default-netif/route-conflict tussen AP en STA.
   Onderzoek: `esp_netif_set_default_netif(ap)`, of STA-netif volledig down halen.
3. **Base-firmware/HW**: is AP-data (client→internet/gateway) ooit bevestigd
   werkend op de stock htotoo-firmware op dit MDK-board? Zo niet, dan is het een
   bestaand probleem van dit board/firmware, niet van de EvilPortal-feature.

## Beste volgende diagnostische stappen
- Voeg een handler toe voor `IP_EVENT_AP_STAIPASSIGNED` + log → bevestigt of de
  server de lease echt afrondt (vs enkel reserveert).
- Test `esp_wifi_set_ps(WIFI_PS_NONE)` en/of AP power-save uit.
- Test één client die vlak na associatie een ping naar 192.168.4.1 doet, met
  verbose lwip/dhcps-logging (`esp_log_level_set` op "esp_netif_lwip"/"dhcps").
- Seriële capture is wispelturig (ESP32-S3 native USB dropt bij wifi-wissels);
  overweeg logging naar het OLED of via de web-UI/WS i.p.v. USB-serial.

## Relevante bestanden
- `Source/main/wifim.cpp` — `apply_portal_ap()` (portal-AP via `config_wifi_apsta`),
  `reconcile_portal_ap()`, `wifi_loop()` (skip STA-reconnect terwijl portal actief).
- `Source/main/apps/ep_app_evilportal.{hpp,cpp}` — on-system app (DNS-hijack,
  captures, clients, custom-HTML upload).
- `Source/main/webserver.h` — captive 404-handler + host-based routing.
- PortaPack-app: `github.com/Xmoo26/mayhem-mdk` → `src/evilportal/`.
