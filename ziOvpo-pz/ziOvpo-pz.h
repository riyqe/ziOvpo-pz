#pragma once

#include "resource.h"
#include <shellapi.h>
#include <string>

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
