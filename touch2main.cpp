// touch2main - spice2x -k DLL: redirect touchscreen (secondary monitor) touches to the primary monitor.
// Usage: spice64.exe -k touch2main_x64.dll   (32-bit spice.exe: touch2main_x86.dll)
// Toggle: click tray icon or Ctrl+Alt+T. Right-click tray icon to turn off until game restarts.
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#define WM_TRAY   (WM_APP + 1)
#define ID_TOGGLE 1001
#define ID_QUIT   1002
#define HK_TOGGLE 1
static HINSTANCE g_hi;
static RECT g_touch, g_main;
static HWND g_overlay;
static bool g_on = true;
static NOTIFYICONDATAW g_nid;
struct Contact { bool used; UINT32 srcId; POINTER_TOUCH_INFO info; };
static Contact g_c[10];
static BOOL CALLBACK EnumMon(HMONITOR m, HDC, LPRECT, LPARAM) {
    MONITORINFO mi; mi.cbSize = sizeof(mi);
    GetMonitorInfoW(m, &mi);
    if (mi.dwFlags & MONITORINFOF_PRIMARY) g_main = mi.rcMonitor;
    else if (IsRectEmpty(&g_touch)) g_touch = mi.rcMonitor;
    return TRUE;
}
static POINT MapPt(POINT p) {
    LONG tw = g_touch.right - g_touch.left, th = g_touch.bottom - g_touch.top;
    LONG mw = g_main.right - g_main.left,   mh = g_main.bottom - g_main.top;
    POINT r = { g_main.left + MulDiv(p.x - g_touch.left, mw, tw), g_main.top + MulDiv(p.y - g_touch.top, mh, th) };
    return r;
}
static void InjectAll() {
    POINTER_TOUCH_INFO arr[10]; UINT32 n = 0;
    for (auto& c : g_c) if (c.used) arr[n++] = c.info;
    if (n) InjectTouchInput(n, arr);
    for (auto& c : g_c)
        if (c.used) {
            if (c.info.pointerInfo.pointerFlags & POINTER_FLAG_UP) c.used = false;
            else c.info.pointerInfo.pointerFlags = POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
        }
}
static void OnPointer(UINT msg, WPARAM wp) {
    UINT32 id = GET_POINTERID_WPARAM(wp);
    POINTER_INFO pi;
    if (!GetPointerInfo(id, &pi) || pi.pointerType != PT_TOUCH) return;
    int slot = -1;
    for (int i = 0; i < 10; i++) if (g_c[i].used && g_c[i].srcId == id) slot = i;
    if (msg == WM_POINTERDOWN && slot < 0)
        for (int i = 0; i < 10; i++) if (!g_c[i].used) { slot = i; break; }
    if (slot < 0) return;
    Contact& c = g_c[slot];
    if (msg == WM_POINTERDOWN) { ZeroMemory(&c, sizeof(c)); c.used = true; c.srcId = id; }
    POINTER_TOUCH_INFO& t = c.info;
    t.pointerInfo.pointerType = PT_TOUCH;
    t.pointerInfo.pointerId = slot;
    t.pointerInfo.ptPixelLocation = MapPt(pi.ptPixelLocation);
    t.touchFlags = TOUCH_FLAG_NONE;
    t.touchMask = TOUCH_MASK_CONTACTAREA;
    POINT p = t.pointerInfo.ptPixelLocation;
    t.rcContact.left = p.x - 2; t.rcContact.top = p.y - 2; t.rcContact.right = p.x + 2; t.rcContact.bottom = p.y + 2;
    t.pointerInfo.pointerFlags =
        msg == WM_POINTERDOWN ? POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT :
        msg == WM_POINTERUP   ? POINTER_FLAG_UP :
                                POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
    InjectAll();
}
static void SetOn(bool on) {
    g_on = on;
    if (!on) { for (auto& c : g_c) if (c.used) c.info.pointerInfo.pointerFlags = POINTER_FLAG_UP; InjectAll(); }
    ShowWindow(g_overlay, on ? SW_SHOWNOACTIVATE : SW_HIDE);
    lstrcpynW(g_nid.szTip, on ? L"Touch -> main screen (ON)" : L"Touch -> main screen (OFF)", 128);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_POINTERACTIVATE: return PA_NOACTIVATE;
    case WM_POINTERDOWN: case WM_POINTERUPDATE: case WM_POINTERUP:
        OnPointer(m, w); return 0;
    case WM_HOTKEY: if (w == HK_TOGGLE) SetOn(!g_on); return 0;
    case WM_TRAY:
        if (LOWORD(l) == WM_LBUTTONUP) SetOn(!g_on);
        else if (LOWORD(l) == WM_RBUTTONUP) {
            HMENU mn = CreatePopupMenu();
            AppendMenuW(mn, MF_STRING | (g_on ? MF_CHECKED : 0), ID_TOGGLE, L"Enabled\tCtrl+Alt+T");
            AppendMenuW(mn, MF_STRING, ID_QUIT, L"Turn off until game restarts");
            POINT p; GetCursorPos(&p);
            int cmd = TrackPopupMenu(mn, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, h, nullptr);
            DestroyMenu(mn);
            if (cmd == ID_TOGGLE) SetOn(!g_on);
            if (cmd == ID_QUIT) DestroyWindow(h);
        }
        return 0;
    case WM_DESTROY:
        if (g_on) SetOn(false);
        UnregisterHotKey(h, HK_TOGGLE);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
static DWORD WINAPI Worker(LPVOID) {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    EnumDisplayMonitors(nullptr, nullptr, EnumMon, 0);
    if (IsRectEmpty(&g_touch) || IsRectEmpty(&g_main)) return 0;
    if (!InitializeTouchInjection(10, TOUCH_FEEDBACK_DEFAULT)) return 0;
    WNDCLASSW wc; ZeroMemory(&wc, sizeof(wc)); wc.lpfnWndProc = WndProc; wc.hInstance = g_hi;
    wc.lpszClassName = L"touch2main_dll"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    g_overlay = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        wc.lpszClassName, L"touch2main", WS_POPUP,
        g_touch.left, g_touch.top, g_touch.right - g_touch.left, g_touch.bottom - g_touch.top,
        nullptr, nullptr, g_hi, nullptr);
    SetLayeredWindowAttributes(g_overlay, 0, 1, LWA_ALPHA);
    ZeroMemory(&g_nid, sizeof(g_nid)); g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_overlay; g_nid.uID = 1; g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY; g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    RegisterHotKey(g_overlay, HK_TOGGLE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T');
    SetOn(true);
    SetTimer(g_overlay, 1, 1000, nullptr);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_TIMER && g_on)
            SetWindowPos(g_overlay, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    return 0;
}
extern "C" BOOL WINAPI DllMain(HINSTANCE hi, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hi = hi;
        DisableThreadLibraryCalls(hi);
        HANDLE t = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_overlay) PostMessageW(g_overlay, WM_CLOSE, 0, 0);
    }
    return TRUE;
}
