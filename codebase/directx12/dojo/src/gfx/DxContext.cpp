#include "DxContext.h"

static D3D12_CPU_DESCRIPTOR_HANDLE RtvHandleAt(DxContext* ctx, UINT index) {
    D3D12_CPU_DESCRIPTOR_HANDLE h = ctx->rtvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += (SIZE_T)index * (SIZE_T)ctx->rtvDescriptorSize;
    return h;
}

static void TransitionResource(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    b.Transition.pResource = res;
    b.Transition.StateBefore = before;
    b.Transition.StateAfter = after;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &b);
}

bool DxContext_Init(DxContext* ctx, HWND hwnd, UINT width, UINT height) {
    ctx->clientWidth = width;
    ctx->clientHeight = height;

#if defined(DEBUG) || defined(_DEBUG)
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
#endif

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    DX_CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

    HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&ctx->device));
    if (FAILED(hr)) {
        Microsoft::WRL::ComPtr<IDXGIAdapter> warp;
        DX_CHECK(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        DX_CHECK(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&ctx->device)));
    }

    DX_CHECK(ctx->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&ctx->fence)));
    ctx->rtvDescriptorSize = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    DX_CHECK(ctx->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&ctx->commandQueue)));

    DX_CHECK(ctx->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&ctx->cmdAlloc)));
    DX_CHECK(ctx->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, ctx->cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&ctx->cmdList)));
    DX_CHECK(ctx->cmdList->Close());

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = kSwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DX_CHECK(ctx->device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&ctx->rtvHeap)));

    DXGI_SWAP_CHAIN_DESC scDesc = {};
    scDesc.BufferCount = kSwapChainBufferCount;
    scDesc.BufferDesc.Width = width;
    scDesc.BufferDesc.Height = height;
    scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferDesc.RefreshRate.Numerator = 60;
    scDesc.BufferDesc.RefreshRate.Denominator = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.OutputWindow = hwnd;
    scDesc.SampleDesc.Count = 1;
    scDesc.SampleDesc.Quality = 0;
    scDesc.Windowed = TRUE;
    scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    Microsoft::WRL::ComPtr<IDXGISwapChain> sc;
    DX_CHECK(factory->CreateSwapChain(ctx->commandQueue.Get(), &scDesc, &sc));
    DX_CHECK(sc.As(&ctx->swapChain));

    for (UINT i = 0; i < kSwapChainBufferCount; i++) {
        DX_CHECK(ctx->swapChain->GetBuffer(i, IID_PPV_ARGS(&ctx->backBuffers[i])));
        ctx->device->CreateRenderTargetView(ctx->backBuffers[i].Get(), nullptr, RtvHandleAt(ctx, i));
    }
    ctx->currentBackBufferIndex = 0;

    ctx->viewport.TopLeftX = 0;
    ctx->viewport.TopLeftY = 0;
    ctx->viewport.Width = (float)width;
    ctx->viewport.Height = (float)height;
    ctx->viewport.MinDepth = 0.0f;
    ctx->viewport.MaxDepth = 1.0f;
    ctx->scissorRect = { 0, 0, (LONG)width, (LONG)height };

    return true;
}

void DxContext_Shutdown(DxContext* ctx) {
    DxContext_FlushCommandQueue(ctx);
    for (UINT i = 0; i < kSwapChainBufferCount; i++)
        ctx->backBuffers[i].Reset();
    ctx->rtvHeap.Reset();
    ctx->swapChain.Reset();
    ctx->cmdList.Reset();
    ctx->cmdAlloc.Reset();
    ctx->commandQueue.Reset();
    ctx->fence.Reset();
    ctx->device.Reset();
}

void DxContext_Resize(DxContext* ctx, UINT width, UINT height) {
    if (width == 0 || height == 0) return;
    DxContext_FlushCommandQueue(ctx);
    for (UINT i = 0; i < kSwapChainBufferCount; i++)
        ctx->backBuffers[i].Reset();
    DX_CHECK(ctx->swapChain->ResizeBuffers(kSwapChainBufferCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
    ctx->currentBackBufferIndex = 0;
    for (UINT i = 0; i < kSwapChainBufferCount; i++) {
        DX_CHECK(ctx->swapChain->GetBuffer(i, IID_PPV_ARGS(&ctx->backBuffers[i])));
        ctx->device->CreateRenderTargetView(ctx->backBuffers[i].Get(), nullptr, RtvHandleAt(ctx, i));
    }
    ctx->clientWidth = width;
    ctx->clientHeight = height;
    ctx->viewport.Width = (float)width;
    ctx->viewport.Height = (float)height;
    ctx->scissorRect = { 0, 0, (LONG)width, (LONG)height };
}

void DxContext_FlushCommandQueue(DxContext* ctx) {
    ctx->fenceValue++;
    DX_CHECK(ctx->commandQueue->Signal(ctx->fence.Get(), ctx->fenceValue));
    if (ctx->fence->GetCompletedValue() < ctx->fenceValue) {
        HANDLE ev = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        DX_CHECK(ctx->fence->SetEventOnCompletion(ctx->fenceValue, ev));
        WaitForSingleObject(ev, INFINITE);
        CloseHandle(ev);
    }
}

void DxContext_BeginFrame(DxContext* ctx) {
    DX_CHECK(ctx->cmdAlloc->Reset());
    DX_CHECK(ctx->cmdList->Reset(ctx->cmdAlloc.Get(), nullptr));
    ID3D12Resource* backBuffer = ctx->backBuffers[ctx->currentBackBufferIndex].Get();
    TransitionResource(ctx->cmdList.Get(), backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    ctx->cmdList->RSSetViewports(1, &ctx->viewport);
    ctx->cmdList->RSSetScissorRects(1, &ctx->scissorRect);
}

void DxContext_ClearRenderTarget(DxContext* ctx, const float colorRGBA[4]) {
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = RtvHandleAt(ctx, ctx->currentBackBufferIndex);
    ctx->cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    ctx->cmdList->ClearRenderTargetView(rtv, colorRGBA, 0, nullptr);
}

void DxContext_EndFrame(DxContext* ctx) {
    ID3D12Resource* backBuffer = ctx->backBuffers[ctx->currentBackBufferIndex].Get();
    TransitionResource(ctx->cmdList.Get(), backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    DX_CHECK(ctx->cmdList->Close());
    ID3D12CommandList* lists[] = { ctx->cmdList.Get() };
    ctx->commandQueue->ExecuteCommandLists(1, lists);
    DX_CHECK(ctx->swapChain->Present(0, 0));
    ctx->currentBackBufferIndex = (ctx->currentBackBufferIndex + 1) % kSwapChainBufferCount;
    DxContext_FlushCommandQueue(ctx);
}
