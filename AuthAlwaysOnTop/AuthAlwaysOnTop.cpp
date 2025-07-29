#include <windows.h>
#include <shellapi.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <strsafe.h>
#include "resource.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001

HINSTANCE hInst;
HWND hWndMain;
NOTIFYICONDATA nid = { 0 };
HWINEVENTHOOK hEventHook = NULL;

// Helper: Checks if a window belongs to CredentialUIBroker.exe
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

// Helper: Brings a window to the foreground and activates it more reliably
void ForceToForeground(HWND hwnd) {
    if (!IsWindow(hwnd)) return;

    DWORD targetThreadId = GetWindowThreadProcessId(hwnd, NULL);
    DWORD currentThreadId = GetCurrentThreadId();

    // Attach input threads if needed
    if (targetThreadId != currentThreadId)
        AttachThreadInput(currentThreadId, targetThreadId, TRUE);

    // Restore/show window
    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);
    else
        ShowWindow(hwnd, SW_SHOW);

    // Force Z-order toggle without activating
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // First attempt to bring window to foreground
    BOOL fgResult = SetForegroundWindow(hwnd);

    // If failed to set foreground, simulate a harmless keypress to unlock foreground rights
    if (!fgResult) {
        INPUT inputs[2] = {};

        // Press SHIFT down
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_SHIFT;
        inputs[0].ki.dwFlags = 0;

        // Release SHIFT
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_SHIFT;
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(2, inputs, sizeof(INPUT));

        Sleep(10); // Short delay

        // Try again
        SetForegroundWindow(hwnd);
    }

    // Extra: Bring window to top, focus and activate
    BringWindowToTop(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    // Detach input threads
    if (targetThreadId != currentThreadId)
        AttachThreadInput(currentThreadId, targetThreadId, FALSE);
}

// Event callback: called when a window is created
void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    if (event == EVENT_OBJECT_CREATE && hwnd && IsWindow(hwnd)) {
        if (IsCredentialUIBrokerWindow(hwnd)) {
            // Debug logging
            OutputDebugString(_T("CredentialUIBroker window detected. Attempting to bring to front.\n"));
            ForceToForeground(hwnd);
        }
    }
}

void ShowTrayMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, _T("Exit"));

    // Bring hidden window to foreground (required for menu to behave correctly)
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
        if (LOWORD(wParam) == ID_TRAY_EXIT) {
            Shell_NotifyIcon(NIM_DELETE, &nid);
            if (hEventHook) UnhookWinEvent(hEventHook);
            PostQuitMessage(0);
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        if (hEventHook) UnhookWinEvent(hEventHook);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

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

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    hInst = hInstance;

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("FastTrayClass");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    // Create a hidden but valid window for tray interaction
    hWndMain = CreateWindowEx(
        0,
        wc.lpszClassName,
        _T("FastTrayApp"),
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0,
        NULL, NULL, hInstance, NULL
    );

    InitTrayIcon(hWndMain);

    // Set up event hook for window creation
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

    return 0;
}
