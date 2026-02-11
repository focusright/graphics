#pragma once

#include "DxCheck.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

constexpr UINT kSwapChainBufferCount = 2;

struct DxContext {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers[kSwapChainBufferCount];
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    UINT64 fenceValue = 0;
    UINT rtvDescriptorSize = 0;
    UINT currentBackBufferIndex = 0;
    UINT clientWidth = 0;
    UINT clientHeight = 0;
    D3D12_VIEWPORT viewport = {};
    D3D12_RECT scissorRect = {};
};

bool DxContext_Init(DxContext* ctx, HWND hwnd, UINT width, UINT height);
void DxContext_Shutdown(DxContext* ctx);
void DxContext_Resize(DxContext* ctx, UINT width, UINT height);
void DxContext_FlushCommandQueue(DxContext* ctx);
void DxContext_BeginFrame(DxContext* ctx);
void DxContext_ClearRenderTarget(DxContext* ctx, const float colorRGBA[4]);
void DxContext_EndFrame(DxContext* ctx);
