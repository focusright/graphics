#include "Win32App.h"
#include "Errors.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <algorithm>
#include <format>
#include <initializer_list>
#include <cwctype>

using Clock = std::chrono::steady_clock;

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

namespace {
constexpr wchar_t mainClass[] = L"GraphicsLabMainWindow";
constexpr wchar_t viewportClass[] = L"GraphicsLabViewport";
constexpr COLORREF uiBackground = RGB(32, 34, 38);
constexpr COLORREF uiSurface = RGB(47, 50, 56);
constexpr COLORREF uiText = RGB(220, 223, 228);
constexpr COLORREF uiAccent = RGB(95, 165, 220);

void FillColor(HDC dc, const RECT& rect, COLORREF color) {
    SetDCBrushColor(dc, color);
    FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

enum : UINT {
    menuExit = 100,
    menuAbout = 101,
    sliderSpeed = 200,
    checkAnimate = 201,
    comboColor = 202,
    buttonReset = 203
};

UINT WindowDpi(HWND window) {
    using GetDpi = UINT (WINAPI*)(HWND);
    static const auto getDpi = reinterpret_cast<GetDpi>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (getDpi && window) {
        return getDpi(window);
    }
    HDC dc = GetDC(nullptr);
    const UINT dpi = dc ? static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX)) : 96;
    if (dc) {
        ReleaseDC(nullptr, dc);
    }
    return dpi;
}

HWND MakeControl(DWORD style, const wchar_t* className, const wchar_t* label,
    HWND parent, UINT id = 0, DWORD extraStyle = 0) {
    HWND control = CreateWindowExW(extraStyle, className, label,
        WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, parent,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)), nullptr);
    Require(control != nullptr, "CreateWindowExW (control)");
    return control;
}
}

Win32App::~Win32App() {
    // Drain submitted work before releasing experiment resources, even on exceptions.
    m_renderer.Shutdown();
    m_experiment.Shutdown();
    // If initialization threw, destroy the window before releasing the controls' font.
    if (m_window && IsWindow(m_window)) {
        DestroyWindow(m_window);
    }
    if (m_font) {
        DeleteObject(m_font);
    }
    if (m_uiBrush) {
        DeleteObject(m_uiBrush);
    }
}

int Win32App::DpiScale(int value) const {
    return MulDiv(value, static_cast<int>(WindowDpi(m_window)), 96);
}

LRESULT CALLBACK Win32App::WindowProc(HWND window, UINT message,
    WPARAM wparam, LPARAM lparam) {
    auto* app = reinterpret_cast<Win32App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        app = static_cast<Win32App*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return app ? app->HandleMessage(window, message, wparam, lparam) :
        DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::ViewportProc(HWND window, UINT message,
    WPARAM wparam, LPARAM lparam) {
    if (message == WM_ERASEBKGND) {
        return 1; // D3D12 paints every frame; prevent GDI flicker.
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void Win32App::UpdateFont() {
    HFONT next = CreateFontW(-MulDiv(9, WindowDpi(m_window), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    Require(next != nullptr, "CreateFontW");

    for (HWND control : {m_title, m_speedText, m_slider, m_animate, m_colorText,
        m_colorSelect, m_reset, m_description}) {
        if (control) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(next), TRUE);
        }
    }
    if (m_font) {
        DeleteObject(m_font);
    }
    m_font = next;
}

void Win32App::CreateControls() {
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_BAR_CLASSES | ICC_WIN95_CLASSES};
    Require(InitCommonControlsEx(&common) != 0, "InitCommonControlsEx");

    m_viewport = CreateWindowExW(0, viewportClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 10, 10, m_window, nullptr, m_instance, nullptr);
    Require(m_viewport != nullptr, "CreateWindowExW (D3D12 viewport)");

    m_status = CreateWindowExW(0, STATUSCLASSNAMEW, L"Initializing D3D12...",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 10, 10, m_window, nullptr, m_instance, nullptr);
    Require(m_status != nullptr, "CreateWindowExW (status bar)");

    // The controls are children of the main HWND, not of the panel.
    // Consequently WM_COMMAND and WM_HSCROLL arrive directly at WindowProc.
    m_title = MakeControl(SS_LEFT, L"STATIC", L"Triangle experiment", m_window);
    m_speedText = MakeControl(SS_LEFT, L"STATIC", L"Rotation speed", m_window);
    m_slider = MakeControl(TBS_HORZ | TBS_AUTOTICKS, TRACKBAR_CLASSW, L"", m_window, sliderSpeed);
    m_animate = MakeControl(BS_AUTOCHECKBOX | WS_TABSTOP, L"BUTTON", L"Animate", m_window, checkAnimate);
    m_colorText = MakeControl(SS_LEFT, L"STATIC", L"Background color", m_window);
    m_colorSelect = MakeControl(CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
        L"COMBOBOX", L"", m_window, comboColor);
    m_reset = MakeControl(BS_PUSHBUTTON | WS_TABSTOP, L"BUTTON", L"Reset", m_window, buttonReset);
    m_description = MakeControl(SS_LEFT, L"STATIC",
        L"Native Win32 controls + Direct3D 12.\r\n"
        L"Shaders are built into the executable.\r\n"
        L"No third-party UI or runtime installer.", m_window);

    // Use documented painting hooks rather than undocumented dark-mode APIs.
    for (HWND control : {m_slider, m_colorSelect, m_status}) {
        SetWindowTheme(control, L"", L"");
    }
    SendMessageW(m_status, SB_SETBKCOLOR, 0, uiBackground);

    SendMessageW(m_slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 400));
    SendMessageW(m_slider, TBM_SETTICFREQ, 100, 0);
    for (const wchar_t* name : {L"Charcoal", L"Navy", L"Forest", L"Plum", L"Light"}) {
        SendMessageW(m_colorSelect, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    }
    UpdateFont();
    SyncControls();
    Layout();
}

void Win32App::SyncControls() {
    SendMessageW(m_slider, TBM_SETPOS, TRUE,
        static_cast<LPARAM>(m_params.rotationSpeed * 100.0f));
    SendMessageW(m_animate, BM_SETCHECK, m_params.animate ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_colorSelect, CB_SETCURSEL, m_params.backgroundColor, 0);
    SetWindowTextW(m_speedText,
        std::format(L"Rotation speed: {:.2f} rad/s", m_params.rotationSpeed).c_str());
}

void Win32App::UpdateStatus() {
    const wchar_t* backend = m_renderer.UsingWarp() ? L"WARP (software)" : L"GPU";
    m_statusText = std::format(L"D3D12 ({})  |  {}  |  {:.2f} rad/s",
        backend, m_params.animate ? L"Animating" : L"Paused", m_params.rotationSpeed);
    SendMessageW(m_status, SB_SETTEXTW, SBT_OWNERDRAW | SBT_NOBORDERS, 0);
}

void Win32App::Layout() {
    if (!m_viewport || !m_status) {
        return;
    }
    RECT client{};
    GetClientRect(m_window, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int statusHeight = DpiScale(25);
    const int contentHeight = std::max(1, height - statusHeight);
    const int panelWidth = std::min(DpiScale(290), std::max(DpiScale(230), width / 3));
    const int viewportWidth = std::max(1, width - panelWidth);

    MoveWindow(m_viewport, 0, 0, viewportWidth, contentHeight, TRUE);
    MoveWindow(m_status, 0, contentHeight, width, statusHeight, TRUE);

    const int x = viewportWidth + DpiScale(15);
    const int innerWidth = std::max(1, panelWidth - DpiScale(30));
    auto place = [&](HWND control, int y, int controlHeight) {
        MoveWindow(control, x, DpiScale(y), innerWidth, DpiScale(controlHeight), TRUE);
    };
    place(m_title, 19, 25);
    place(m_speedText, 62, 23);
    place(m_slider, 91, 43);
    place(m_animate, 150, 28);
    place(m_colorText, 205, 22);
    place(m_colorSelect, 234, 200); // Drop-down height includes the expanded list.
    place(m_reset, 295, 32);
    place(m_description, 369, 86);
}

LRESULT Win32App::HandleMessage(HWND window, UINT message,
    WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_MENUCHAR: {
        const auto menu = reinterpret_cast<HMENU>(lparam);
        for (int index = 0; index < GetMenuItemCount(menu); ++index) {
            MENUITEMINFOW item{sizeof(MENUITEMINFOW)};
            item.fMask = MIIM_DATA;
            if (GetMenuItemInfoW(menu, index, TRUE, &item) && item.dwItemData) {
                const auto* marker = wcschr(reinterpret_cast<const wchar_t*>(item.dwItemData), L'&');
                if (marker && marker[1] && std::towupper(marker[1]) == std::towupper(static_cast<wchar_t>(LOWORD(wparam)))) {
                    return MAKELRESULT(index, MNC_EXECUTE);
                }
            }
        }
        return MAKELRESULT(0, MNC_IGNORE);
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        const auto dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, uiText);
        SetBkColor(dc, uiBackground);
        return reinterpret_cast<LRESULT>(m_uiBrush);
    }

    case WM_MEASUREITEM: {
        auto* item = reinterpret_cast<MEASUREITEMSTRUCT*>(lparam);
        if (item->CtlType == ODT_MENU) {
            HDC dc = GetDC(window);
            const auto oldFont = SelectObject(dc, m_font ? m_font : GetStockObject(DEFAULT_GUI_FONT));
            SIZE size{};
            const auto* label = reinterpret_cast<const wchar_t*>(item->itemData);
            GetTextExtentPoint32W(dc, label, lstrlenW(label), &size);
            item->itemWidth = size.cx + DpiScale(24);
            item->itemHeight = std::max(static_cast<int>(size.cy) + DpiScale(6), DpiScale(22));
            SelectObject(dc, oldFont);
            ReleaseDC(window, dc);
            return TRUE;
        }
        if (item->CtlID == comboColor) {
            item->itemHeight = DpiScale(22);
            return TRUE;
        }
        break;
    }

    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
        if (item->CtlType == ODT_MENU) {
            const int saved = SaveDC(item->hDC);
            FillColor(item->hDC, item->rcItem, (item->itemState & ODS_SELECTED) ? uiSurface : uiBackground);
            SetTextColor(item->hDC, uiText);
            SetBkMode(item->hDC, TRANSPARENT);
            SelectObject(item->hDC, m_font ? m_font : GetStockObject(DEFAULT_GUI_FONT));
            RECT rect = item->rcItem;
            rect.left += DpiScale(8);
            DrawTextW(item->hDC, reinterpret_cast<const wchar_t*>(item->itemData), -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            RestoreDC(item->hDC, saved);
            return TRUE;
        }
        if (item->hwndItem == m_colorSelect || item->hwndItem == m_status) {
            const int saved = SaveDC(item->hDC);
            FillColor(item->hDC, item->rcItem, (item->itemState & ODS_SELECTED) ? uiSurface : uiBackground);
            SetTextColor(item->hDC, uiText);
            SetBkMode(item->hDC, TRANSPARENT);
            if (m_font) {
                SelectObject(item->hDC, m_font);
            }
            wchar_t label[64]{};
            const wchar_t* text = m_statusText.c_str();
            if (item->hwndItem == m_colorSelect) {
                if (item->itemID != static_cast<UINT>(-1)) {
                    SendMessageW(m_colorSelect, CB_GETLBTEXT, item->itemID, reinterpret_cast<LPARAM>(label));
                }
                text = label;
            }
            RECT rect = item->rcItem;
            rect.left += DpiScale(5);
            DrawTextW(item->hDC, text, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            if (item->itemState & ODS_FOCUS) {
                DrawFocusRect(item->hDC, &item->rcItem);
            }
            RestoreDC(item->hDC, saved);
            return TRUE;
        }
        break;
    }

    case WM_NOTIFY: {
        auto* draw = reinterpret_cast<NMCUSTOMDRAW*>(lparam);
        if (draw->hdr.code != NM_CUSTOMDRAW) {
            break;
        }
        if (draw->hdr.hwndFrom == m_slider) {
            if (draw->dwDrawStage == CDDS_PREPAINT) {
                return CDRF_NOTIFYITEMDRAW;
            }
            if (draw->dwDrawStage == CDDS_ITEMPREPAINT) {
                if (draw->dwItemSpec == TBCD_CHANNEL || draw->dwItemSpec == TBCD_THUMB) {
                    FillColor(draw->hdc, draw->rc, draw->dwItemSpec == TBCD_THUMB ? uiAccent : uiSurface);
                }
                return CDRF_SKIPDEFAULT;
            }
        } else if ((draw->hdr.hwndFrom == m_reset || draw->hdr.hwndFrom == m_animate) && draw->dwDrawStage == CDDS_PREPAINT) {
            const int saved = SaveDC(draw->hdc);
            RECT rect = draw->rc;
            FillColor(draw->hdc, rect, draw->hdr.hwndFrom == m_reset ? uiSurface : uiBackground);
            if (draw->hdr.hwndFrom == m_animate) {
                RECT box{rect.left, rect.top + (rect.bottom - rect.top - DpiScale(14)) / 2, rect.left + DpiScale(14), 0};
                box.bottom = box.top + DpiScale(14);
                FillColor(draw->hdc, box, uiSurface);
                if (SendMessageW(m_animate, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    InflateRect(&box, -DpiScale(3), -DpiScale(3));
                    FillColor(draw->hdc, box, uiAccent);
                }
                rect.left += DpiScale(20);
            }
            wchar_t label[64]{};
            GetWindowTextW(draw->hdr.hwndFrom, label, static_cast<int>(std::size(label)));
            SetTextColor(draw->hdc, uiText);
            SetBkMode(draw->hdc, TRANSPARENT);
            SelectObject(draw->hdc, m_font);
            DrawTextW(draw->hdc, label, -1, &rect, DT_VCENTER | DT_SINGLELINE | (draw->hdr.hwndFrom == m_reset ? DT_CENTER : DT_LEFT));
            if (draw->uItemState & CDIS_FOCUS) {
                InflateRect(&rect, -2, -2);
                DrawFocusRect(draw->hdc, &rect);
            }
            RestoreDC(draw->hdc, saved);
            return CDRF_SKIPDEFAULT;
        }
        break;
    }

    case WM_SIZE:
        Layout();
        return 0;

    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize = {DpiScale(740), DpiScale(510)};
        return 0;
    }

    case WM_DPICHANGED: {
        const auto* requested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(window, nullptr, requested->left, requested->top,
            requested->right - requested->left, requested->bottom - requested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateFont();
        Layout();
        return 0;
    }

    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lparam) == m_slider) {
            m_params.rotationSpeed = static_cast<float>(SendMessageW(m_slider, TBM_GETPOS, 0, 0)) / 100.0f;
            SetWindowTextW(m_speedText,
                std::format(L"Rotation speed: {:.2f} rad/s", m_params.rotationSpeed).c_str());
            UpdateStatus();
            return 0;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case menuExit:
            DestroyWindow(window);
            return 0;
        case menuAbout:
            MessageBoxW(window,
                L"GraphicsLab - a small Win32 + D3D12 graphics sandbox.\n"
                L"Built with modern C++ and no external runtime dependencies.",
                L"About GraphicsLab", MB_OK | MB_ICONINFORMATION);
            return 0;
        case checkAnimate:
            if (HIWORD(wparam) == BN_CLICKED) {
                m_params.animate = SendMessageW(m_animate, BM_GETCHECK, 0, 0) == BST_CHECKED;
                UpdateStatus();
            }
            return 0;
        case comboColor:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                m_params.backgroundColor = static_cast<int>(SendMessageW(m_colorSelect, CB_GETCURSEL, 0, 0));
            }
            return 0;
        case buttonReset:
            if (HIWORD(wparam) == BN_CLICKED) {
                m_params = {};
                m_experiment.Reset();
                SyncControls();
                UpdateStatus();
            }
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        m_window = nullptr;
        return DefWindowProcW(window, message, wparam, lparam);
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int Win32App::Run(HINSTANCE instance, int showCommand) {
    m_instance = instance;
    m_uiBrush = CreateSolidBrush(uiBackground);
    Require(m_uiBrush != nullptr, "CreateSolidBrush (UI background)");

    WNDCLASSEXW mainWindowClass{sizeof(WNDCLASSEXW)};
    mainWindowClass.style = CS_HREDRAW | CS_VREDRAW;
    mainWindowClass.lpfnWndProc = WindowProc;
    mainWindowClass.hInstance = instance;
    mainWindowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    mainWindowClass.hbrBackground = m_uiBrush;
    mainWindowClass.lpszClassName = mainClass;
    Require(RegisterClassExW(&mainWindowClass) != 0, "RegisterClassExW (main window)");

    WNDCLASSEXW viewportWindowClass{sizeof(WNDCLASSEXW)};
    viewportWindowClass.lpfnWndProc = ViewportProc;
    viewportWindowClass.hInstance = instance;
    viewportWindowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    viewportWindowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    viewportWindowClass.lpszClassName = viewportClass;
    Require(RegisterClassExW(&viewportWindowClass) != 0, "RegisterClassExW (viewport)");

    HMENU menu = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    HMENU helpMenu = CreatePopupMenu();
    Require(menu && fileMenu && helpMenu, "CreateMenu");
    AppendMenuW(fileMenu, MF_OWNERDRAW, menuExit, L"E&xit");
    AppendMenuW(helpMenu, MF_OWNERDRAW, menuAbout, L"&About");
    AppendMenuW(menu, MF_POPUP | MF_OWNERDRAW, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
    AppendMenuW(menu, MF_POPUP | MF_OWNERDRAW, reinterpret_cast<UINT_PTR>(helpMenu), L"&Help");
    MENUINFO menuInfo{sizeof(MENUINFO)};
    menuInfo.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    menuInfo.hbrBack = m_uiBrush;
    SetMenuInfo(menu, &menuInfo);
    // Keep menu labels available to accessibility clients for owner-drawn items.
    auto setMenuLabel = [](HMENU target, UINT position, const wchar_t* label) {
        MENUITEMINFOW item{sizeof(MENUITEMINFOW)};
        item.fMask = MIIM_STRING;
        item.dwTypeData = const_cast<wchar_t*>(label);
        SetMenuItemInfoW(target, position, TRUE, &item);
    };
    setMenuLabel(menu, 0, L"&File");
    setMenuLabel(menu, 1, L"&Help");
    setMenuLabel(fileMenu, 0, L"E&xit");
    setMenuLabel(helpMenu, 0, L"&About");

    m_window = CreateWindowExW(0, mainClass, L"GraphicsLab - D3D12 Experiment",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
        1160, 710, nullptr, menu, instance, this);
    if (!m_window) {
        DestroyMenu(menu);
        Require(false, "CreateWindowExW (main window)");
    }

    const BOOL darkTitleBar = TRUE;
    // Older Windows versions may reject this optional appearance attribute.
    DwmSetWindowAttribute(m_window, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkTitleBar, sizeof(darkTitleBar));
    CreateControls();
    m_renderer.Initialize(m_viewport);
    m_experiment.Initialize(m_renderer.Device());
    UpdateStatus();

    ShowWindow(m_window, showCommand);
    UpdateWindow(m_window);

    auto lastFrame = Clock::now();
    MSG message{};
    bool quit = false;
    while (!quit) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                quit = true;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (quit) {
            break;
        }

        if (IsIconic(m_window)) {
            WaitMessage();
            lastFrame = Clock::now();
            continue;
        }

        RECT client{};
        GetClientRect(m_viewport, &client);
        const UINT width = static_cast<UINT>(std::max(0L, client.right - client.left));
        const UINT height = static_cast<UINT>(std::max(0L, client.bottom - client.top));
        if (width == 0 || height == 0) {
            WaitMessage();
            lastFrame = Clock::now();
            continue;
        }
        m_renderer.Resize(width, height);

        const auto now = Clock::now();
        const double seconds = std::clamp(std::chrono::duration<double>(now - lastFrame).count(), 0.0, 0.1);
        lastFrame = now;
        m_experiment.Update(seconds, m_params);
        m_renderer.Render(m_experiment, m_params);
    }

    m_renderer.Shutdown();
    m_experiment.Shutdown();
    return static_cast<int>(message.wParam);
}
