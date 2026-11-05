#include "D3D12Renderer.h"
#include "TriangleExperiment.h"
#include "Errors.h"

#include <algorithm>
#include <array>

using Microsoft::WRL::ComPtr;

namespace {
constexpr std::array<std::array<float, 4>, 5> clearColors{{
    {0.09f, 0.11f, 0.15f, 1.0f}, // Charcoal
    {0.07f, 0.16f, 0.28f, 1.0f}, // Navy
    {0.13f, 0.20f, 0.13f, 1.0f}, // Forest
    {0.23f, 0.10f, 0.22f, 1.0f}, // Plum
    {0.88f, 0.90f, 0.94f, 1.0f}  // Light
}};

D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}
}

D3D12Renderer::~D3D12Renderer() {
    Shutdown();
}

void D3D12Renderer::Initialize(HWND viewport) {
    m_viewport = viewport;
    RECT rect{};
    Require(GetClientRect(viewport, &rect) != 0, "GetClientRect (viewport)");
    m_width = std::max(1L, rect.right - rect.left);
    m_height = std::max(1L, rect.bottom - rect.top);

#ifdef _DEBUG
    // The debug layer is optional. Missing Graphics Tools must not prevent launch.
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer();
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");

    for (UINT index = 0; ; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (!adapter) {
            break;
        }
        DXGI_ADAPTER_DESC1 desc{};
        Check(adapter->GetDesc1(&desc), "GetDesc1");
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)))) {
            break;
        }
    }

    if (!m_device) {
        ComPtr<IDXGIAdapter> warp;
        Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)), "EnumWarpAdapter");
        Check(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)), "D3D12CreateDevice (WARP fallback)");
        m_usingWarp = true;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_queue)), "CreateCommandQueue");
    Check(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_allocator)), "CreateCommandAllocator");
    Check(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_allocator.Get(), nullptr, IID_PPV_ARGS(&m_commands)), "CreateCommandList");
    Check(m_commands->Close(), "Close initial command list");

    DXGI_SWAP_CHAIN_DESC1 swapDesc{};
    swapDesc.Width = m_width;
    swapDesc.Height = m_height;
    swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = bufferCount;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> initialSwapChain;
    Check(factory->CreateSwapChainForHwnd(m_queue.Get(), viewport, &swapDesc,
        nullptr, nullptr, &initialSwapChain), "CreateSwapChainForHwnd");
    Check(initialSwapChain.As(&m_swapChain), "Query IDXGISwapChain3");
    Check(factory->MakeWindowAssociation(viewport, DXGI_MWA_NO_ALT_ENTER),
        "MakeWindowAssociation");

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = bufferCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    Check(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvHeap)),
        "CreateDescriptorHeap (RTV)");
    m_rtvStride = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    CreateRenderTargets();

    Check(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)),
        "CreateFence");
    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Require(m_fenceEvent != nullptr, "CreateEventW (GPU fence)");
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Renderer::RtvAt(UINT index) const {
    auto handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * m_rtvStride;
    return handle;
}

void D3D12Renderer::CreateRenderTargets() {
    for (UINT index = 0; index < bufferCount; ++index) {
        Check(m_swapChain->GetBuffer(index, IID_PPV_ARGS(&m_backBuffers[index])),
            "GetBuffer (swap chain)");
        m_device->CreateRenderTargetView(m_backBuffers[index].Get(), nullptr, RtvAt(index));
    }
}

void D3D12Renderer::WaitForGpu() {
    if (!m_queue || !m_fence || !m_fenceEvent) {
        return;
    }

    const UINT64 value = m_nextFenceValue++;
    Check(m_queue->Signal(m_fence.Get(), value), "Signal GPU fence");
    if (m_fence->GetCompletedValue() < value) {
        Check(m_fence->SetEventOnCompletion(value, m_fenceEvent), "SetEventOnCompletion");
        Require(WaitForSingleObject(m_fenceEvent, 10000) == WAIT_OBJECT_0,
            "WaitForSingleObject (GPU fence)");
    }
}

void D3D12Renderer::Resize(UINT width, UINT height) {
    if (!m_swapChain || width == 0 || height == 0 || (width == m_width && height == m_height)) {
        return;
    }

    WaitForGpu();
    for (auto& buffer : m_backBuffers) {
        buffer.Reset();
    }

    Check(m_swapChain->ResizeBuffers(bufferCount, width, height,
        DXGI_FORMAT_R8G8B8A8_UNORM, 0), "ResizeBuffers");
    m_width = width;
    m_height = height;
    CreateRenderTargets();
}

void D3D12Renderer::Render(const TriangleExperiment& experiment, const ExperimentParams& params) {
    if (!m_swapChain || m_width == 0 || m_height == 0) {
        return;
    }

    // Intentionally wait for each frame; simple, correct synchronization for a teaching sandbox.
    Check(m_allocator->Reset(), "Reset command allocator");
    Check(m_commands->Reset(m_allocator.Get(), nullptr), "Reset command list");

    const UINT index = m_swapChain->GetCurrentBackBufferIndex();
    ID3D12Resource* buffer = m_backBuffers[index].Get();
    auto toRender = Transition(buffer, D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commands->ResourceBarrier(1, &toRender);

    const D3D12_VIEWPORT view{0.0f, 0.0f, static_cast<float>(m_width),
        static_cast<float>(m_height), 0.0f, 1.0f};
    const D3D12_RECT scissor{0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
    m_commands->RSSetViewports(1, &view);
    m_commands->RSSetScissorRects(1, &scissor);

    const auto rtv = RtvAt(index);
    m_commands->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    const int color = std::clamp(params.backgroundColor, 0,
        static_cast<int>(clearColors.size()) - 1);
    m_commands->ClearRenderTargetView(rtv, clearColors[color].data(), 0, nullptr);
    experiment.Record(m_commands.Get());

    auto toPresent = Transition(buffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_commands->ResourceBarrier(1, &toPresent);
    Check(m_commands->Close(), "Close command list");
    ID3D12CommandList* lists[]{m_commands.Get()};
    m_queue->ExecuteCommandLists(1, lists);
    Check(m_swapChain->Present(1, 0), "Present");
    WaitForGpu();
}

void D3D12Renderer::Shutdown() {
    if (m_queue && m_fence && m_fenceEvent) {
        // In a healthy session, ensure no submitted GPU commands use our resources.
        // Shutdown must never throw, including from the destructor.
        try {
            WaitForGpu();
        } catch (...) {
            OutputDebugStringW(L"GraphicsLab: GPU fence wait failed during shutdown.\n");
        }
    }
    for (auto& buffer : m_backBuffers) {
        buffer.Reset();
    }
    m_rtvHeap.Reset();
    m_swapChain.Reset();
    m_commands.Reset();
    m_allocator.Reset();
    m_queue.Reset();
    m_fence.Reset();
    m_device.Reset();
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    m_viewport = nullptr;
}
