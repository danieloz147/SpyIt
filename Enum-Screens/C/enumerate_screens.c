#include <windows.h>
#include <stdio.h>

// Simple callback function to enumerate display monitors
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    int* pCount = (int*)dwData;
    (*pCount)++;
    
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(MONITORINFO);
    
    if (GetMonitorInfoW(hMonitor, &mi))
    {
        printf("[+] Monitor %d %s: Resolution: %ldx%ld\n", *pCount - 1, (mi.dwFlags & MONITORINFOF_PRIMARY) ? "[PRIMARY]" : "         " ,mi.rcMonitor.right - mi.rcMonitor.left,mi.rcMonitor.bottom - mi.rcMonitor.top);
    }
    return TRUE;
}

int main()
{
    int screenCount = 0;
    printf("[?] Enumerating display monitors...\n\n");
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&screenCount);
    printf("\n[!] Total screens detected: %d\n", screenCount);
    return 0;
}
