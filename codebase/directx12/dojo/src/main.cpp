// Belt 0: Boot + Clear. Layers: (1) Execution & Sync, (3) Fixed-Function.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "platform/Win32Window.h"
#include "gfx/DxContext.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
#if defined(DEBUG) || defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    Win32Window window = {};
    if (!Win32Window_Create(&window, hInstance, L"D3D12 Dojo – Belt 0")) {
        MessageBoxW(nullptr, L"Window creation failed.", L"Dojo", MB_OK);
        return 1;
    }

    DxContext dx = {};
    if (!DxContext_Init(&dx, window.hwnd, (UINT)window.clientWidth, (UINT)window.clientHeight)) {
        MessageBoxW(nullptr, L"D3D12 init failed.", L"Dojo", MB_OK);
        Win32Window_Destroy(&window);
        return 1;
    }

    const float clearColor[4] = { 0.0f, 0.0f, 0.2f, 1.0f }; // dark blue

    while (window.running) {
        Win32Window_PumpMessages(&window);
        if (!window.running) break;

        RECT rc;
        GetClientRect(window.hwnd, &rc);
        UINT w = (UINT)(rc.right - rc.left), h = (UINT)(rc.bottom - rc.top);
        if (w != dx.clientWidth || h != dx.clientHeight) {
            if (w > 0 && h > 0) DxContext_Resize(&dx, w, h);
        }

        DxContext_BeginFrame(&dx);
        DxContext_ClearRenderTarget(&dx, clearColor);
        DxContext_EndFrame(&dx);
    }

    DxContext_Shutdown(&dx);
    Win32Window_Destroy(&window);
    return 0;
}
