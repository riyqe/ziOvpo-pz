#include <windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <rpc.h>
#include <vector>
#include <string>
#include <cstdlib>

#include "../common/AppConfig.h"
#include "../rpc/ServiceControl.h"

#if defined(_M_ARM64)
extern "C" {
#include "../rpc/ServiceControl_s_arm64.c"
}
#elif defined(_M_IX86)
extern "C" {
#include "../rpc/ServiceControl_s_win32.c"
}
#else
extern "C" {
#include "../rpc/ServiceControl_s_x64.c"
}
#endif

#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Userenv.lib")
#pragma comment(lib, "Advapi32.lib")

SERVICE_STATUS_HANDLE g_serviceHandle = nullptr;
SERVICE_STATUS g_serviceStatus{};
CRITICAL_SECTION g_lock;
std::vector<PROCESS_INFORMATION> g_apps;

extern "C" void* __RPC_USER MIDL_user_allocate(size_t size)
{
    return malloc(size);
}

extern "C" void __RPC_USER MIDL_user_free(void* p)
{
    free(p);
}

std::wstring GetSelfDirectory()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring s = path;
    size_t p = s.find_last_of(L"\\/");
    if (p == std::wstring::npos)
    {
        return L".";
    }
    return s.substr(0, p);
}

std::wstring GetTrayAppPath()
{
    return GetSelfDirectory() + L"\\TrayApp.exe";
}

bool IsRunning(HANDLE process)
{
    if (!process)
    {
        return false;
    }

    DWORD code = 0;
    if (!GetExitCodeProcess(process, &code))
    {
        return false;
    }

    return code == STILL_ACTIVE;
}

bool HasAppForSession(DWORD sessionId)
{
    EnterCriticalSection(&g_lock);

    bool found = false;
    for (const auto& pi : g_apps)
    {
        if (!pi.hProcess)
        {
            continue;
        }

        DWORD pidSession = 0;
        if (ProcessIdToSessionId(pi.dwProcessId, &pidSession) && pidSession == sessionId && IsRunning(pi.hProcess))
        {
            found = true;
            break;
        }
    }

    LeaveCriticalSection(&g_lock);
    return found;
}

bool StartAppInSession(DWORD sessionId)
{
    if (sessionId == 0)
    {
        return false;
    }

    if (HasAppForSession(sessionId))
    {
        return true;
    }

    HANDLE userToken = nullptr;
    if (!WTSQueryUserToken(sessionId, &userToken))
    {
        return false;
    }

    HANDLE primaryToken = nullptr;
    BOOL tokenOk = DuplicateTokenEx(
        userToken,
        TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID,
        nullptr,
        SecurityImpersonation,
        TokenPrimary,
        &primaryToken);
    CloseHandle(userToken);

    if (!tokenOk)
    {
        return false;
    }

    LPVOID env = nullptr;
    CreateEnvironmentBlock(&env, primaryToken, FALSE);

    std::wstring appPath = GetTrayAppPath();
    std::wstring command = L"\"" + appPath + L"\" --hidden";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    BOOL created = CreateProcessAsUserW(
        primaryToken,
        appPath.c_str(),
        command.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_UNICODE_ENVIRONMENT,
        env,
        nullptr,
        &si,
        &pi);

    if (env)
    {
        DestroyEnvironmentBlock(env);
    }

    CloseHandle(primaryToken);

    if (!created)
    {
        return false;
    }

    CloseHandle(pi.hThread);

    EnterCriticalSection(&g_lock);
    g_apps.push_back(pi);
    LeaveCriticalSection(&g_lock);

    return true;
}

void StartAppsInCurrentSessions()
{
    PWTS_SESSION_INFO sessions = nullptr;
    DWORD count = 0;

    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count))
    {
        return;
    }

    for (DWORD i = 0; i < count; ++i)
    {
        DWORD id = sessions[i].SessionId;
        if (id == 0)
        {
            continue;
        }

        StartAppInSession(id);
    }

    WTSFreeMemory(sessions);
}

void StopAllApps()
{
    EnterCriticalSection(&g_lock);

    for (auto& pi : g_apps)
    {
        if (pi.hProcess && IsRunning(pi.hProcess))
        {
            TerminateProcess(pi.hProcess, 0);
        }

        if (pi.hProcess)
        {
            CloseHandle(pi.hProcess);
            pi.hProcess = nullptr;
        }
    }

    g_apps.clear();

    LeaveCriticalSection(&g_lock);
}

void SetState(DWORD state, DWORD acceptedControls)
{
    g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_serviceStatus.dwCurrentState = state;
    g_serviceStatus.dwControlsAccepted = acceptedControls;
    g_serviceStatus.dwWin32ExitCode = NO_ERROR;
    g_serviceStatus.dwServiceSpecificExitCode = 0;
    g_serviceStatus.dwCheckPoint = 0;
    g_serviceStatus.dwWaitHint = 0;

    if (g_serviceHandle)
    {
        SetServiceStatus(g_serviceHandle, &g_serviceStatus);
    }
}

bool StartRpc()
{
    RPC_STATUS s = RpcServerUseProtseqEpW(
        (RPC_WSTR)L"ncalrpc",
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        (RPC_WSTR)kRpcEndpoint,
        nullptr);

    if (s != RPC_S_OK)
    {
        return false;
    }

    s = RpcServerRegisterIf2(
        ServiceControl_v1_0_s_ifspec,
        nullptr,
        nullptr,
        RPC_IF_ALLOW_LOCAL_ONLY,
        RPC_C_LISTEN_MAX_CALLS_DEFAULT,
        (unsigned)-1,
        nullptr);

    return s == RPC_S_OK;
}

void RpcRequestStop(handle_t)
{
    SetState(SERVICE_STOP_PENDING, SERVICE_ACCEPT_SESSIONCHANGE);
    RpcMgmtStopServerListening(nullptr);
}

DWORD WINAPI ServiceHandler(DWORD control, DWORD eventType, LPVOID eventData, LPVOID)
{
    if (control == SERVICE_CONTROL_SESSIONCHANGE && eventType == WTS_SESSION_LOGON)
    {
        auto* data = reinterpret_cast<WTSSESSION_NOTIFICATION*>(eventData);
        if (data)
        {
            StartAppInSession(data->dwSessionId);
        }
    }

    return NO_ERROR;
}

void WINAPI ServiceMain(DWORD, LPWSTR*)
{
    InitializeCriticalSection(&g_lock);

    g_serviceHandle = RegisterServiceCtrlHandlerExW(kServiceName, ServiceHandler, nullptr);
    if (!g_serviceHandle)
    {
        DeleteCriticalSection(&g_lock);
        return;
    }

    SetState(SERVICE_START_PENDING, 0);

    if (!StartRpc())
    {
        SetState(SERVICE_STOPPED, 0);
        DeleteCriticalSection(&g_lock);
        return;
    }

    StartAppsInCurrentSessions();
    SetState(SERVICE_RUNNING, SERVICE_ACCEPT_SESSIONCHANGE);

    RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, FALSE);

    RpcServerUnregisterIf(ServiceControl_v1_0_s_ifspec, nullptr, FALSE);
    StopAllApps();
    SetState(SERVICE_STOPPED, 0);

    DeleteCriticalSection(&g_lock);
}

int wmain()
{
    SERVICE_TABLE_ENTRYW table[] =
    {
        { const_cast<LPWSTR>(kServiceName), ServiceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcherW(table))
    {
        return 1;
    }

    return 0;
}
