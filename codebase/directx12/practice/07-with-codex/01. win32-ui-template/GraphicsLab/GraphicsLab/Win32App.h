#pragma once

#include <windows.h>
#include <chrono>
#include <string>

#include "D3D12Renderer.h"
#include "Experiment.h"
#include "TriangleExperiment.h"

class Win32App {
public:
    ~Win32App();
    int Run(HINSTANCE instance, int showCommand);

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK ViewportProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    void CreateControls();
    void Layout();
    void UpdateFont();
    void SyncControls();
    void UpdateStatus();
    int DpiScale(int value) const;

    HINSTANCE m_instance = nullptr;
    HWND m_window = nullptr;
    HWND m_viewport = nullptr;
    HWND m_status = nullptr;
    HWND m_title = nullptr;
    HWND m_speedText = nullptr;
    HWND m_slider = nullptr;
    HWND m_animate = nullptr;
    HWND m_colorText = nullptr;
    HWND m_colorSelect = nullptr;
    HWND m_reset = nullptr;
    HWND m_description = nullptr;
    HFONT m_font = nullptr;
    HBRUSH m_uiBrush = nullptr;
    std::wstring m_statusText;

    ExperimentParams m_params;
    D3D12Renderer m_renderer;
    TriangleExperiment m_experiment;
};
