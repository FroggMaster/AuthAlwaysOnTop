#include <windows.h>
#include <shellapi.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <strsafe.h>
#include <shlwapi.h>
#include "resource.h"

#pragma comment(lib, "Shlwapi.lib")

#define WM_TRAYICON      (WM_USER + 1)
#define ID_TRAY_EXIT     1001
#define ID_TRAY_TOGGLE   1002
#define ID_TRAY_HELP     1003
#define ID_HOTKEY_TOGGLE 2001

#ifndef MOD_WIN
#define MOD_WIN 0x0008
#endif

HINSTANCE hInst;
HWND hWndMain;
NOTIFYICONDATA nid = { 0 };
HWINEVENTHOOK hEventHook = NULL;

// Named mutex to ensure a single instance
HANDLE hMutex = NULL;

// Config persistence
bool gTrayIconVisible = true;
TCHAR configPath[MAX_PATH] = { 0 };
const TCHAR* CONFIG_SECTION = _T("Settings");
const TCHAR* CONFIG_KEY = _T("TrayIconVisible");

// Resolve config.ini to EXE directory
void GetConfigPath() {
    TCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    PathRemoveFileSpec(exePath);
    PathCombine(configPath, exePath, _T("config.ini"));
}

// Checks if a window belongs to CredentialUIBroker.exe
bool IsCredentialUIBrokerWindow(HWND hWnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe = { 0 };
    pe.dwSize = sizeof(PROCESSENTRY32);
    bool result = false;
    if (Process32First(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == pid && _tcsicmp(pe.szExeFile, _T("CredentialUIBroker.exe")) == 0) {
                result = true;
                break;
            }
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return result;
}

// Brings a window to the foreground reliably
void ForceToForeground(HWND hwnd) {
    if (!IsWindow(hwnd)) return;

    DWORD targetThreadId = GetWindowThreadProcessId(hwnd, NULL);
    DWORD currentThreadId = GetCurrentThreadId();

    if (targetThreadId != currentThreadId)
        AttachThreadInput(currentThreadId, targetThreadId, TRUE);

    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);
    else
        ShowWindow(hwnd, SW_SHOW);

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    BOOL fgResult = SetForegroundWindow(hwnd);

    if (!fgResult) {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_SHIFT;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_SHIFT;
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT));
        Sleep(10);
        SetForegroundWindow(hwnd);
    }

    BringWindowToTop(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    if (targetThreadId != currentThreadId)
        AttachThreadInput(currentThreadId, targetThreadId, FALSE);
}

// Config read/write
void LoadTrayIconSetting() {
    TCHAR value[8] = { 0 };
    GetPrivateProfileString(CONFIG_SECTION, CONFIG_KEY, _T("1"), value, 8, configPath);
    gTrayIconVisible = (_ttoi(value) != 0);
}

void SaveTrayIconSetting() {
    WritePrivateProfileString(CONFIG_SECTION, CONFIG_KEY, gTrayIconVisible ? _T("1") : _T("0"), configPath);
}

// Tray icon
void InitTrayIcon(HWND hwnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON5));
    _tcscpy_s(nid.szTip, _T("AuthAlwaysOnTop"));
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void SetTrayIconVisible(HWND hwnd, bool visible) {
    if (visible && !gTrayIconVisible) {
        InitTrayIcon(hwnd);
    }
    else if (!visible && gTrayIconVisible) {
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }
    gTrayIconVisible = visible;
    SaveTrayIconSetting();
}

// Event callback
void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    if (event == EVENT_OBJECT_CREATE && hwnd && IsWindow(hwnd)) {
        if (IsCredentialUIBrokerWindow(hwnd)) {
            OutputDebugString(_T("CredentialUIBroker window detected. Attempting to bring to front.\n"));
            ForceToForeground(hwnd);
        }
    }
}

// Tray menu
void ShowTrayMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_TOGGLE, gTrayIconVisible ? _T("Hide Tray Icon") : _T("Show Tray Icon"));
    AppendMenu(hMenu, MF_STRING, ID_TRAY_HELP, _T("Help"));
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, _T("Exit"));

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowTrayMenu(hwnd);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_TOGGLE:
            SetTrayIconVisible(hwnd, !gTrayIconVisible);
            break;
        case ID_TRAY_HELP:
            MessageBox(hwnd,
                _T("AuthAlwaysOnTop Help\n\n")
                _T("Hotkey: Ctrl + Win + Alt + ScrollLock\n")
                _T("Use this hotkey to toggle the tray icon visibility at any time.\n\n")
                _T("You can also use the tray menu or config.ini to control the tray icon."),
                _T("Help"),
                MB_OK | MB_ICONINFORMATION);
            break;
        case ID_TRAY_EXIT:
            Shell_NotifyIcon(NIM_DELETE, &nid);
            if (hEventHook) UnhookWinEvent(hEventHook);
            UnregisterHotKey(hwnd, ID_HOTKEY_TOGGLE);
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_HOTKEY:
        if (wParam == ID_HOTKEY_TOGGLE) {
            SetTrayIconVisible(hwnd, !gTrayIconVisible);
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        if (hEventHook) UnhookWinEvent(hEventHook);
        UnregisterHotKey(hwnd, ID_HOTKEY_TOGGLE);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Ensure only one instance runs
    hMutex = CreateMutex(NULL, FALSE, _T("AuthAlwaysOnTop_Mutex"));
    if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        //MessageBox(NULL, _T("The application is already running."), _T("AuthAlwaysOnTop"), MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    hInst = hInstance;
    GetConfigPath();  // Resolve full path to config.ini

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("FastTrayClass");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    hWndMain = CreateWindowEx(
        0,
        wc.lpszClassName,
        _T("FastTrayApp"),
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0,
        NULL, NULL, hInstance, NULL
    );

    LoadTrayIconSetting();
    if (gTrayIconVisible) {
        InitTrayIcon(hWndMain);
    }

    RegisterHotKey(hWndMain, ID_HOTKEY_TOGGLE, MOD_CONTROL | MOD_WIN | MOD_ALT, VK_SCROLL);

    hEventHook = SetWinEventHook(
        EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE,
        NULL, WinEventProc,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Release the mutex on exit
    if (hMutex) {
        CloseHandle(hMutex);
    }

    return 0;
}
