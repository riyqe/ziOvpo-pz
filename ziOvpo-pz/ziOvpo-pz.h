#pragma once

#include "resource.h"
#include <shellapi.h>
#include <string>
#include <vector>

namespace av
{
    struct AvDatabase;
    struct ScanFinding;
}

extern av::AvDatabase g_avDatabase;
extern std::vector<av::ScanFinding> g_lastScanResults;
extern bool g_avDatabaseLoaded;

extern bool g_scheduleEnabled;
extern unsigned int g_scheduleIntervalMinutes;
extern std::vector<std::wstring> g_monitoredDirectories;

void LoadAvDatabase();
void UpdateAvStatusText();
void ScanSelectedFile(HWND owner);
void ScanSelectedFolder(HWND owner);
void ScanAllFixedDrives(HWND owner);
void ConfigureScheduledScan(HWND owner);
void ToggleScheduledScan(HWND owner);
void AddMonitoringDirectory(HWND owner);
void ShowMonitoredDirectories(HWND owner);
void ShowScanResults(HWND owner);


#define MAX_LOADSTRING 100

inline constexpr UINT WM_TRAYICON = WM_APP + 1;
inline constexpr UINT ID_TRAY_ICON = 1;
inline constexpr UINT IDM_TRAY_OPEN = 40001;
inline constexpr UINT IDM_TRAY_EXIT = 40002;

extern HINSTANCE hInst;
extern WCHAR szTitle[MAX_LOADSTRING];
extern WCHAR szWindowClass[MAX_LOADSTRING];

extern UINT g_taskbarCreatedMessage;
extern NOTIFYICONDATAW g_notifyIconData;
extern bool g_trayAdded;
extern bool g_isExiting;
extern HANDLE g_singleInstanceMutex;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, bool startHidden);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

std::wstring BuildMutexNamePerUser();
void ShowMainWindow(HWND hWnd);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon();
void ShowTrayContextMenu(HWND hWnd);
