#include "framework.h"
#include "ziOvpo-pz.h"
#include <lmcons.h>
#include <tlhelp32.h>
#include <rpc.h>
#include <cstdlib>

#include "common/AppConfig.h"
#include "rpc/ServiceControl.h"

#if defined(_M_ARM64)
extern "C" {
#include "rpc/ServiceControl_c_arm64.c"
}
#elif defined(_M_IX86)
extern "C" {
#include "rpc/ServiceControl_c_win32.c"
}
#else
extern "C" {
#include "rpc/ServiceControl_c_x64.c"
}
#endif

#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Advapi32.lib")

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

UINT g_taskbarCreatedMessage = 0;
NOTIFYICONDATAW g_notifyIconData{};
bool g_trayAdded = false;
bool g_isExiting = false;
HANDLE g_singleInstanceMutex = nullptr;

extern "C" void* __RPC_USER MIDL_user_allocate(size_t size)
{
    return malloc(size);
}

extern "C" void __RPC_USER MIDL_user_free(void* p)
{
    free(p);
}

std::wstring BuildMutexNamePerUser()
{
    wchar_t userName[UNLEN + 1]{};
    DWORD size = UNLEN + 1;
    if (!GetUserNameW(userName, &size))
    {
        return L"Local\\ziOvpo-pz-singleinstance-default";
    }

    return std::wstring(L"Local\\ziOvpo-pz-singleinstance-") + userName;
}

void ShowMainWindow(HWND hWnd)
{
    ShowWindow(hWnd, SW_SHOW);
    ShowWindow(hWnd, SW_RESTORE);
    SetForegroundWindow(hWnd);
}

void AddTrayIcon(HWND hWnd)
{
    ZeroMemory(&g_notifyIconData, sizeof(g_notifyIconData));
    g_notifyIconData.cbSize = sizeof(g_notifyIconData);
    g_notifyIconData.hWnd = hWnd;
    g_notifyIconData.uID = ID_TRAY_ICON;
    g_notifyIconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_notifyIconData.uCallbackMessage = WM_TRAYICON;
    g_notifyIconData.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SMALL));
    lstrcpynW(g_notifyIconData.szTip, L"ziOvpo-pz", ARRAYSIZE(g_notifyIconData.szTip));

    g_trayAdded = Shell_NotifyIconW(NIM_ADD, &g_notifyIconData) == TRUE;
    if (g_trayAdded)
    {
        g_notifyIconData.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &g_notifyIconData);
    }
}

void RemoveTrayIcon()
{
    if (g_trayAdded)
    {
        Shell_NotifyIconW(NIM_DELETE, &g_notifyIconData);
        g_trayAdded = false;
    }
}

void ShowTrayContextMenu(HWND hWnd)
{
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu)
    {
        return;
    }

    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Открыть");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"Выход");

    POINT pt{};
    GetCursorPos(&pt);

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, nullptr);
    PostMessageW(hWnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);
}

bool WaitServiceRunning(SC_HANDLE service)
{
    for (int i = 0; i < 200; ++i)
    {
        SERVICE_STATUS_PROCESS status{};
        DWORD bytesNeeded = 0;
        if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytesNeeded))
        {
            return false;
        }

        if (status.dwCurrentState == SERVICE_RUNNING)
        {
            return true;
        }

        Sleep(100);
    }

    return false;
}

bool ServiceMustAllowRun()
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm)
    {
        return false;
    }

    SC_HANDLE service = OpenServiceW(scm, kServiceName, SERVICE_QUERY_STATUS | SERVICE_START);
    if (!service)
    {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS_PROCESS status{};
    DWORD bytesNeeded = 0;
    bool allowRun = false;

    if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytesNeeded))
    {
        if (status.dwCurrentState == SERVICE_RUNNING)
        {
            allowRun = true;
        }
        else
        {
            StartServiceW(service, 0, nullptr);
            WaitServiceRunning(service);
            allowRun = false;
        }
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return allowRun;
}

DWORD GetParentPid(DWORD pid)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    DWORD parent = 0;
    if (Process32FirstW(snap, &pe))
    {
        do
        {
            if (pe.th32ProcessID == pid)
            {
                parent = pe.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return parent;
}

std::wstring GetProcessPath(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
    {
        return L"";
    }

    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    std::wstring result;

    if (QueryFullProcessImageNameW(process, 0, path, &size))
    {
        result = path;
    }

    CloseHandle(process);
    return result;
}

bool IsParentService()
{
    DWORD parent = GetParentPid(GetCurrentProcessId());
    if (parent == 0)
    {
        return false;
    }

    std::wstring path = GetProcessPath(parent);
    if (path.empty())
    {
        return false;
    }

    size_t p = path.find_last_of(L"\\/");
    std::wstring fileName = (p == std::wstring::npos) ? path : path.substr(p + 1);

    return _wcsicmp(fileName.c_str(), kServiceExeName) == 0;
}

bool StopServiceByRpc()
{
    RPC_WSTR bindingText = nullptr;
    handle_t binding = nullptr;

    RPC_STATUS s = RpcStringBindingComposeW(
        nullptr,
        (RPC_WSTR)L"ncalrpc",
        nullptr,
        (RPC_WSTR)kRpcEndpoint,
        nullptr,
        &bindingText);

    if (s != RPC_S_OK)
    {
        return false;
    }

    s = RpcBindingFromStringBindingW(bindingText, &binding);
    RpcStringFreeW(&bindingText);
    if (s != RPC_S_OK)
    {
        return false;
    }

    bool ok = true;

    RpcTryExcept
    {
        RpcRequestStop(binding);
    }
    RpcExcept(1)
    {
        ok = false;
    }
    RpcEndExcept;

    RpcBindingFree(&binding);

    return ok;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    const bool debugRun = IsDebuggerPresent() != FALSE;

    if (!debugRun && !ServiceMustAllowRun())
    {
        return FALSE;
    }

    if (!debugRun && !IsParentService())
    {
        return FALSE;
    }

    const std::wstring mutexName = BuildMutexNamePerUser();
    g_singleInstanceMutex = CreateMutexW(nullptr, FALSE, mutexName.c_str());
    if (!g_singleInstanceMutex || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (g_singleInstanceMutex)
        {
            CloseHandle(g_singleInstanceMutex);
            g_singleInstanceMutex = nullptr;
        }
        return FALSE;
    }

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_ZIOVPOPZ, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    const bool startHidden = (lpCmdLine != nullptr) &&
        (wcsstr(lpCmdLine, L"--hidden") != nullptr || wcsstr(lpCmdLine, L"/hidden") != nullptr);

    if (!InitInstance(hInstance, nCmdShow, startHidden))
    {
        CloseHandle(g_singleInstanceMutex);
        g_singleInstanceMutex = nullptr;
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ZIOVPOPZ));

    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (g_singleInstanceMutex)
    {
        CloseHandle(g_singleInstanceMutex);
        g_singleInstanceMutex = nullptr;
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ZIOVPOPZ));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_ZIOVPOPZ);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, bool startHidden)
{
    hInst = hInstance;

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    if (!startHidden)
    {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
    }

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == g_taskbarCreatedMessage)
    {
        AddTrayIcon(hWnd);
        return 0;
    }

    switch (message)
    {
    case WM_CREATE:
        AddTrayIcon(hWnd);
        break;
    case WM_CLOSE:
        if (!g_isExiting)
        {
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        }
        break;
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
        case IDM_TRAY_EXIT:
            g_isExiting = true;
            StopServiceByRpc();
            DestroyWindow(hWnd);
            break;
        case IDM_TRAY_OPEN:
            ShowMainWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_TRAYICON:
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONUP:
            ShowMainWindow(hWnd);
            break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayContextMenu(hWnd);
            break;
        default:
            break;
        }
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        UNREFERENCED_PARAMETER(hdc);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        RemoveTrayIcon();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
