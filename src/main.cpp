// ============================================================================
// main.cpp - tray resident host + Win+Tab launcher
// ============================================================================
#include "Flip3DComp.h"

#include <shellapi.h>

#include <iterator>
#include <memory>

namespace
{
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kStartFlipMessage = WM_APP + 2;
constexpr UINT kActivateFlipMessage = WM_APP + 3;
constexpr UINT kTrayExitCommand = 1001;
constexpr UINT_PTR kFrameTimerId = 1;
constexpr UINT kFrameTimerMs = 16;

HINSTANCE g_instance = nullptr;
HWND g_hostWindow = nullptr;
HHOOK g_keyboardHook = nullptr;
UINT g_taskbarCreatedMessage = 0;

bool g_running = true;
bool g_winTabActive = false;
std::unique_ptr<Flip3DCompApp> g_app;

bool AnyWinKeyDown()
{
    return (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0
        || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
}

void SuppressStartMenuForWinGesture()
{
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(static_cast<UINT>(std::size(inputs)), inputs, sizeof(INPUT));
}

void AddTrayIcon(HWND hwnd)
{
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kTrayMessage;
    nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"Flip3DComp - Win+Tab");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hwnd)
{
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void ShowTrayMenu(HWND hwnd)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    AppendMenuW(menu, MF_STRING, kTrayExitCommand, L"Exit");

    POINT pt = {};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    const UINT command = TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x,
        pt.y,
        0,
        hwnd,
        nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    if (command != 0)
        PostMessageW(hwnd, WM_COMMAND, command, 0);
}

void ShowInitError(const wchar_t* message)
{
    MessageBoxW(nullptr,
        message ? message : L"Failed to initialize Flip3DComp.",
        L"Flip3DComp", MB_OK | MB_ICONERROR);
}

void StartFlipSession()
{
    if (g_app && g_app->IsWindowAlive())
    {
        g_app->Show();
        PostMessageW(g_app->WindowHandle(), WM_KEYDOWN, VK_TAB, 0);
        return;
    }

    auto app = std::make_unique<Flip3DCompApp>();
    app->SetPostQuitOnDestroy(false);

    if (!app->Initialize(g_instance))
    {
        ShowInitError(app->InitErrorMessage());
        return;
    }

    app->Show();
    g_app = std::move(app);
    SetTimer(g_hostWindow, kFrameTimerId, kFrameTimerMs, nullptr);
}

void ActivateFlipSelection()
{
    if (g_app && g_app->IsWindowAlive())
        g_app->ActivateSelected();
}

void StopFlipSessionNow()
{
    if (!g_app)
        return;

    if (g_app->IsWindowAlive())
        DestroyWindow(g_app->WindowHandle());

    g_app.reset();
    KillTimer(g_hostWindow, kFrameTimerId);
}

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        const auto* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        const UINT vkCode = kbd->vkCode;
        const bool keyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        const bool keyUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
        const bool isWinKey = vkCode == VK_LWIN || vkCode == VK_RWIN;

        if (keyDown && vkCode == VK_TAB && AnyWinKeyDown())
        {
            g_winTabActive = true;
            SuppressStartMenuForWinGesture();
            PostMessageW(g_hostWindow, kStartFlipMessage, 0, 0);
            return 1;
        }

        if (g_winTabActive && keyUp && isWinKey)
        {
            SuppressStartMenuForWinGesture();
            g_winTabActive = false;
            PostMessageW(g_hostWindow, kActivateFlipMessage, 0, 0);
            return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
        }
    }

    return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

LRESULT CALLBACK HostWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == g_taskbarCreatedMessage)
    {
        AddTrayIcon(hwnd);
        return 0;
    }

    switch (message)
    {
    case kStartFlipMessage:
        StartFlipSession();
        return 0;

    case kActivateFlipMessage:
        ActivateFlipSelection();
        return 0;

    case kTrayMessage:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU)
        {
            ShowTrayMenu(hwnd);
            return 0;
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == kTrayExitCommand)
        {
            g_running = false;
            StopFlipSessionNow();
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_TIMER:
        if (wParam == kFrameTimerId)
        {
            if (g_app && g_app->IsWindowAlive())
            {
                g_app->TickFrame();
            }
            else
            {
                g_app.reset();
                KillTimer(hwnd, kFrameTimerId);
            }
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool CreateHostWindow()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = g_instance;
    wc.lpfnWndProc = HostWndProc;
    wc.lpszClassName = L"Flip3DCompTrayHostWindow";
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    g_hostWindow = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Flip3DComp Tray Host",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        g_instance,
        nullptr);

    return g_hostWindow != nullptr;
}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    g_instance = hInstance;
    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    if (!CreateHostWindow())
    {
        ShowInitError(L"Failed to create the tray host window.");
        return 1;
    }

    AddTrayIcon(g_hostWindow);

    g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, hInstance, 0);
    if (!g_keyboardHook)
    {
        RemoveTrayIcon(g_hostWindow);
        DestroyWindow(g_hostWindow);
        ShowInitError(L"Failed to install the Win+Tab keyboard hook.");
        return 1;
    }

    MSG msg = {};
    while (g_running && GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_keyboardHook)
        UnhookWindowsHookEx(g_keyboardHook);

    StopFlipSessionNow();
    RemoveTrayIcon(g_hostWindow);
    DestroyWindow(g_hostWindow);
    return static_cast<int>(msg.wParam);
}
