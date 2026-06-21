#include <windows.h>
#include "beacon.h"

// BOF-style imports for USER32 APIs
DECLSPEC_IMPORT BOOL WINAPI USER32$EnumDisplayMonitors(HDC, LPCRECT, MONITORENUMPROC, LPARAM);
DECLSPEC_IMPORT BOOL WINAPI USER32$GetMonitorInfoW(HMONITOR, LPMONITORINFO);

#define EnumDisplayMonitors USER32$EnumDisplayMonitors
#define GetMonitorInfoW     USER32$GetMonitorInfoW

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
   int* pCount = (int*)dwData;
    (*pCount)++;
   
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);

    if (GetMonitorInfoW(hMonitor, &mi)) 
    {
        BeaconPrintf(CALLBACK_OUTPUT, "[+] Monitor %d %s: Resolution: %ldx%ld\n", *pCount - 1, (mi.dwFlags & MONITORINFOF_PRIMARY) ? "[PRIMARY]" : "         " ,mi.rcMonitor.right - mi.rcMonitor.left,mi.rcMonitor.bottom - mi.rcMonitor.top);
    }
    return TRUE;
}

void go(char* args, int len) {
    int screenCount = 0;
    BeaconPrintf(CALLBACK_OUTPUT, "[?] Enumerating display monitors...\n\n");
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&screenCount);
    BeaconPrintf(CALLBACK_OUTPUT, "\n[!] Total screens detected: %d\n", screenCount);
}