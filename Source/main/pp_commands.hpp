#pragma once

// sattrack
#define PPCMD_SATTRACK_DATA 0xa000
#define PPCMD_SATTRACK_SETSAT 0xa001
#define PPCMD_SATTRACK_SETMGPS 0xa002
// ir
#define PPCMD_IRTX_SENDIR 0xa003
#define PPCMD_IRTX_GETLASTRCVIR 0xa004
// Wifi settings app
#define PPCMD_WIFI_SET_STA 0xa005
#define PPCMD_WIFI_SET_AP 0xa006
#define PPCMD_WIFI_GET_CONFIG 0xa007
#define PPCMD_WIFI_STARTSCAN 0xa008
#define PPCMD_WIFI_STOPSCAN 0xa009
#define PPCMD_WIFI_GETSCANRESULT 0xa00a
// esp manager
#define PPCMD_AIRPLANE_MODE 0xa00b
// appmgr - apps on esp
#define PPCMD_APPMGR_APPMGR 0xa00c
#define PPCMD_APPMGR_APPCMD 0xa00d
// evil portal - dedicated portal AP (kept separate from the management AP)
#define PPCMD_EVILPORTAL_SET_AP 0xa00e
#define PPCMD_EVILPORTAL_GET_AP 0xa00f
// evil portal - list fetch (index sent as uint16, one entry returned per read)
#define PPCMD_EVILPORTAL_GET_CLIENT 0xa010
#define PPCMD_EVILPORTAL_GET_CAPTURE 0xa011
// evil portal - custom HTML upload: payload [uint16 offset][bytes];
//   offset 0 resets the buffer, offset 0xFFFF commits & activates the custom page
#define PPCMD_EVILPORTAL_SET_HTML 0xa012
// evil portal - readback diagnostic: returns [uint16 customHtmlLen][uint8 useCustomHtml][uint8 firstByte]
#define PPCMD_EVILPORTAL_GET_HTMLLEN 0xa013

// wifi analyzer - passive 2.4GHz scan; GET_ENTRY sends uint16 index (write) and
// returns one wifi_ap_entry_t (read); RESCAN forces a fresh scan
#define PPCMD_WIFIANALYZER_GET_ENTRY 0xa020
#define PPCMD_WIFIANALYZER_RESCAN 0xa021
// wifi analyzer - enable/disable auto rescan (payload byte: 1=on, 0=off)
#define PPCMD_WIFIANALYZER_SET_AUTO 0xa022

// probe sniffer - GET_ENTRY sends uint16 index / returns one probe_entry_t;
// SET_AUTO byte 1=capture,0=pause; CLEAR wipes the list
#define PPCMD_PROBESNIFFER_GET_ENTRY 0xa030
#define PPCMD_PROBESNIFFER_SET_AUTO 0xa031
#define PPCMD_PROBESNIFFER_CLEAR 0xa032

// pmkid capture - GET_ENTRY (uint16 index -> pmkid_entry_t), SET_AUTO (1=capture,0=pause), CLEAR
#define PPCMD_PMKID_GET_ENTRY 0xa040
#define PPCMD_PMKID_SET_AUTO 0xa041
#define PPCMD_PMKID_CLEAR 0xa042
// pmkid capture - lock to a channel (payload byte: 0=hop all, 1-13=stay)
#define PPCMD_PMKID_SET_CHANNEL 0xa043
// pmkid capture - fetch hashcat line text in chunks
#define PPCMD_PMKID_GET_LINELEN 0xa044   // returns uint16 length of line reqLineIdx
#define PPCMD_PMKID_SET_LINEPOS 0xa045   // write [uint16 idx][uint16 offset]
#define PPCMD_PMKID_GET_LINECHUNK 0xa046  // returns up to 96 bytes at (idx,offset)
// pmkid capture - send a deauth burst on the current channel (force reconnect)
#define PPCMD_PMKID_DEAUTH 0xa047

// standalone deauth tool
#define PPCMD_DEAUTH_GET_ENTRY 0xa050   // uint16 index -> deauth_ap_t
#define PPCMD_DEAUTH_RESCAN 0xa051
#define PPCMD_DEAUTH_SET_TARGET 0xa052  // uint16 index of the AP to attack
#define PPCMD_DEAUTH_SET_ATTACK 0xa053  // byte 1=start, 0=stop

// BLE scanner
#define PPCMD_BLESCAN_GET_ENTRY 0xa060  // uint16 index -> ble_dev_t
#define PPCMD_BLESCAN_SET_AUTO 0xa061   // byte 1=scan, 0=pause
#define PPCMD_BLESCAN_CLEAR 0xa062

// BLE spam
#define PPCMD_BLESPAM_SET_MODE 0xa070   // byte 0=Apple,1=FastPair,2=SwiftPair,3=all,4=custom
#define PPCMD_BLESPAM_SET_ACTIVE 0xa071  // byte 1=start, 0=stop
#define PPCMD_BLESPAM_ADD 0xa072         // [idx][off][bytes] custom payload upload
#define PPCMD_BLESPAM_COMMIT 0xa073      // [count] activate custom payloads
#define PPCMD_BLESPAM_CLEAR_CUSTOM 0xa074

// drone detector
#define PPCMD_DRONE_GET_ENTRY 0xa080  // uint16 index -> drone_t
#define PPCMD_DRONE_CLEAR 0xa081