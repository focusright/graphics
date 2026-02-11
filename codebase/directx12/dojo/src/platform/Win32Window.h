#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct Win32Window {
    HWND hwnd = nullptr;
    int clientWidth = 800;
    int clientHeight = 600;
    bool running = true;
};

bool Win32Window_Create(Win32Window* w, HINSTANCE hInstance, const wchar_t* title);
void Win32Window_Destroy(Win32Window* w);
void Win32Window_PumpMessages(Win32Window* w);
