#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include "Experiment.h"

class TriangleExperiment;

class D3D12Renderer {
public:
    D3D12Renderer() = default;
    D3D12Renderer(const D3D12Renderer&) = delete;
    D3D12Renderer& operator=(const D3D12Renderer&) = delete;
    ~D3D12Renderer();

    void Initialize(HWND viewport);
    void Resize(UINT width, UINT height);
    void Render(const TriangleExperiment& experiment, const ExperimentParams& params);
    void Shutdown();

    ID3D12Device* Device() const { return m_device.Get(); }
    bool UsingWarp() const { return m_usingWarp; }

private:
    static constexpr UINT bufferCount = 2;
    void CreateRenderTargets();
    void WaitForGpu();
    D3D12_CPU_DESCRIPTOR_HANDLE RtvAt(UINT index) const;

    HWND m_viewport = nullptr;
    UINT m_width = 0;
    UINT m_height = 0;
    UINT m_rtvStride = 0;
    bool m_usingWarp = false;
    UINT64 m_nextFenceValue = 1;
    HANDLE m_fenceEvent = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commands;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[bufferCount];
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
};
