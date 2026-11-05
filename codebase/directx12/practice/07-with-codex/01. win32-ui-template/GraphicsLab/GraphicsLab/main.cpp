#include "Win32App.h"

#include <exception>
#include <string>
#include <string_view>

// Request Common Controls v6 without shipping additional UI libraries.
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {
std::wstring Utf8ToWide(std::string_view value) {
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) {
        return L"Unrecognized application error";
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), length);
    return result;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    // Resolve the newer DPI API dynamically to retain compatibility with early Windows 10.
    using SetDpiContext = BOOL (WINAPI*)(DPI_AWARENESS_CONTEXT);
    const auto user32 = GetModuleHandleW(L"user32.dll");
    const auto setDpiContext = reinterpret_cast<SetDpiContext>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (!setDpiContext || !setDpiContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        SetProcessDPIAware();
    }

    try {
        Win32App app;
        return app.Run(instance, showCommand);
    } catch (const std::exception& error) {
        const auto text = Utf8ToWide(error.what());
        MessageBoxW(nullptr, text.c_str(), L"GraphicsLab startup / rendering error",
            MB_OK | MB_ICONERROR);
        return 1;
    }
}
