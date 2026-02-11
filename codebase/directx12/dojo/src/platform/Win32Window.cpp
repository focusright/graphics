#define WIN32_LEAN_AND_MEAN
#include "Win32Window.h"

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32Window* w = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (w && msg == WM_DESTROY) {
        w->running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool Win32Window_Create(Win32Window* w, HINSTANCE hInstance, const wchar_t* title) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DojoWindowClass";
    if (!RegisterClassExW(&wc)) return false;

    RECT rc = { 0, 0, w->clientWidth, w->clientHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    w->hwnd = CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);
    if (!w->hwnd) return false;

    SetWindowLongPtrW(w->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(w));
    ShowWindow(w->hwnd, SW_SHOW);
    return true;
}

void Win32Window_Destroy(Win32Window* w) {
    if (w->hwnd) {
        DestroyWindow(w->hwnd);
        w->hwnd = nullptr;
    }
}

void Win32Window_PumpMessages(Win32Window* w) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) w->running = false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
