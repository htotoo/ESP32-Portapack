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