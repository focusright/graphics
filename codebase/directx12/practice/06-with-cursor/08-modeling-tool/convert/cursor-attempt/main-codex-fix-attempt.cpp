#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;
using DirectX::XMFLOAT4X4;
using DirectX::XMMATRIX;
using DirectX::XMVECTOR;
using Microsoft::WRL::ComPtr;
using namespace D3DX12;

namespace
{
constexpr UINT kFrameCount = 2;
constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;
constexpr int kMaxQuadSlots = 100;
constexpr int kMaxDrawableQuads = kMaxQuadSlots - 1;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
};

struct SceneConstants
{
    XMFLOAT4X4 worldViewProj;
};

struct Quad
{
    int x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;
    float r, g, b;
    int state;
    int total;
};

HWND g_hwnd = nullptr;

ComPtr<ID3D12Device> g_device;
ComPtr<IDXGISwapChain3> g_swapChain;
ComPtr<ID3D12CommandQueue> g_commandQueue;
ComPtr<ID3D12CommandAllocator> g_commandAllocator;
ComPtr<ID3D12GraphicsCommandList> g_commandList;
ComPtr<ID3D12Fence> g_fence;
HANDLE g_fenceEvent = nullptr;
UINT64 g_fenceValue = 0;
UINT g_frameIndex = 0;

ComPtr<ID3D12DescriptorHeap> g_rtvHeap;
ComPtr<ID3D12DescriptorHeap> g_dsvHeap;
ComPtr<ID3D12DescriptorHeap> g_cbvHeap;
UINT g_rtvDescriptorSize = 0;

ComPtr<ID3D12Resource> g_renderTargets[kFrameCount];
ComPtr<ID3D12Resource> g_depthStencil;

ComPtr<ID3D12RootSignature> g_rootSignature;
ComPtr<ID3D12PipelineState> g_trianglePipelineState;
ComPtr<ID3D12PipelineState> g_linePipelineState;

ComPtr<ID3D12Resource> g_gridVertexBuffer;
ComPtr<ID3D12Resource> g_cubeVertexBuffer;
ComPtr<ID3D12Resource> g_quadVertexBuffer;
ComPtr<ID3D12Resource> g_constantBuffer;

D3D12_VERTEX_BUFFER_VIEW g_gridBufferView = {};
D3D12_VERTEX_BUFFER_VIEW g_cubeBufferView = {};
D3D12_VERTEX_BUFFER_VIEW g_quadBufferView = {};

SceneConstants* g_constantBufferData = nullptr;
Vertex* g_quadVertexData = nullptr;

Quad g_quads[kMaxQuadSlots] = {};
int g_cursorX = 0;
int g_cursorY = 0;
int g_cursorZ = 0;
int g_currentQuadIndex = 0;

void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::runtime_error("D3D12 call failed.");
    }
}

void WaitForGpu()
{
    const UINT64 fenceToWaitFor = ++g_fenceValue;
    ThrowIfFailed(g_commandQueue->Signal(g_fence.Get(), fenceToWaitFor));

    if (g_fence->GetCompletedValue() < fenceToWaitFor)
    {
        ThrowIfFailed(g_fence->SetEventOnCompletion(fenceToWaitFor, g_fenceEvent));
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

void CreateUploadVertexBuffer(const Vertex* vertices, UINT vertexCount, ComPtr<ID3D12Resource>& resource, D3D12_VERTEX_BUFFER_VIEW& view)
{
    const UINT bufferSize = vertexCount * sizeof(Vertex);
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    ThrowIfFailed(g_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource)));

    void* mappedData = nullptr;
    const D3D12_RANGE readRange = {0, 0};
    ThrowIfFailed(resource->Map(0, &readRange, &mappedData));
    std::memcpy(mappedData, vertices, bufferSize);
    resource->Unmap(0, nullptr);

    view.BufferLocation = resource->GetGPUVirtualAddress();
    view.SizeInBytes = bufferSize;
    view.StrideInBytes = sizeof(Vertex);
}

XMMATRIX BuildSceneTransform()
{
    const XMVECTOR axis = DirectX::XMVector3Normalize(DirectX::XMVectorSet(1.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX rotation = DirectX::XMMatrixRotationAxis(axis, DirectX::XMConvertToRadians(40.0f));
    const XMMATRIX translation = DirectX::XMMatrixTranslation(-13.0f, 0.0f, 45.0f);
    return rotation * translation;
}

XMMATRIX BuildProjectionMatrix()
{
    return DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XMConvertToRadians(35.0f),
        static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight),
        0.1f,
        1000.0f);
}

void UpdateSceneConstants(const XMMATRIX& worldMatrix)
{
    const XMMATRIX worldViewProj = worldMatrix * BuildSceneTransform() * BuildProjectionMatrix();
    DirectX::XMStoreFloat4x4(&g_constantBufferData->worldViewProj, DirectX::XMMatrixTranspose(worldViewProj));
}

void UpdateQuadVertexBuffer()
{
    int vertexIndex = 0;

    for (int i = 1; i <= g_quads[0].total && i < kMaxQuadSlots; ++i)
    {
        const XMFLOAT4 color(g_quads[i].r, g_quads[i].g, g_quads[i].b, 1.0f);

        g_quadVertexData[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x1), static_cast<float>(g_quads[i].y1), static_cast<float>(g_quads[i].z1)), color};
        g_quadVertexData[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x2), static_cast<float>(g_quads[i].y2), static_cast<float>(g_quads[i].z2)), color};
        g_quadVertexData[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x3), static_cast<float>(g_quads[i].y3), static_cast<float>(g_quads[i].z3)), color};

        g_quadVertexData[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x1), static_cast<float>(g_quads[i].y1), static_cast<float>(g_quads[i].z1)), color};
        g_quadVertexData[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x3), static_cast<float>(g_quads[i].y3), static_cast<float>(g_quads[i].z3)), color};
        g_quadVertexData[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x4), static_cast<float>(g_quads[i].y4), static_cast<float>(g_quads[i].z4)), color};
    }
}

void AddQuadPoint()
{
    g_quads[0].state++;
    if (g_quads[0].state > 4)
    {
        g_quads[0].state = 1;
    }

    const int state = g_quads[0].state;
    if (state == 1)
    {
        if (g_quads[0].total >= kMaxDrawableQuads)
        {
            return;
        }

        g_quads[0].total++;
        g_currentQuadIndex = g_quads[0].total;
    }

    Quad& quad = g_quads[g_currentQuadIndex];

    if (state == 1)
    {
        quad.x1 = quad.x2 = quad.x3 = quad.x4 = g_cursorX;
        quad.y1 = quad.y2 = quad.y3 = quad.y4 = g_cursorY;
        quad.z1 = quad.z2 = quad.z3 = quad.z4 = g_cursorZ;
    }

    if (state >= 1)
    {
        quad.x2 = g_cursorX;
        quad.y2 = g_cursorY;
        quad.z2 = g_cursorZ;
    }
    if (state >= 2)
    {
        quad.x3 = g_cursorX;
        quad.y3 = g_cursorY;
        quad.z3 = g_cursorZ;
    }
    if (state >= 3)
    {
        quad.x4 = g_cursorX;
        quad.y4 = g_cursorY;
        quad.z4 = g_cursorZ;
    }
}

void SetCurrentQuadColor(float r, float g, float b)
{
    if (g_currentQuadIndex <= 0 || g_currentQuadIndex >= kMaxQuadSlots)
    {
        return;
    }

    g_quads[g_currentQuadIndex].r = r;
    g_quads[g_currentQuadIndex].g = g;
    g_quads[g_currentQuadIndex].b = b;
}

void PopulateCommandList()
{
    ThrowIfFailed(g_commandAllocator->Reset());
    ThrowIfFailed(g_commandList->Reset(g_commandAllocator.Get(), nullptr));

    g_commandList->SetGraphicsRootSignature(g_rootSignature.Get());

    ID3D12DescriptorHeap* heaps[] = {g_cbvHeap.Get()};
    g_commandList->SetDescriptorHeaps(1, heaps);
    g_commandList->SetGraphicsRootDescriptorTable(0, g_cbvHeap->GetGPUDescriptorHandleForHeapStart());

    const D3D12_VIEWPORT viewport = {
        0.0f,
        0.0f,
        static_cast<float>(kWindowWidth),
        static_cast<float>(kWindowHeight),
        0.0f,
        1.0f};
    const D3D12_RECT scissorRect = {0, 0, kWindowWidth, kWindowHeight};
    g_commandList->RSSetViewports(1, &viewport);
    g_commandList->RSSetScissorRects(1, &scissorRect);

    const auto toRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(
        g_renderTargets[g_frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    g_commandList->ResourceBarrier(1, &toRenderTarget);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(g_frameIndex) * g_rtvDescriptorSize;
    const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = g_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    g_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = {0.1f, 0.1f, 0.1f, 1.0f};
    g_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    g_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    g_commandList->SetPipelineState(g_linePipelineState.Get());
    g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    g_commandList->IASetVertexBuffers(0, 1, &g_gridBufferView);
    UpdateSceneConstants(DirectX::XMMatrixIdentity());
    g_commandList->DrawInstanced(40 * 2, 1, 0, 0);

    if (g_quads[0].total > 0)
    {
        g_commandList->SetPipelineState(g_trianglePipelineState.Get());
        g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_commandList->IASetVertexBuffers(0, 1, &g_quadBufferView);
        UpdateSceneConstants(DirectX::XMMatrixIdentity());
        g_commandList->DrawInstanced(g_quads[0].total * 6, 1, 0, 0);
    }

    g_commandList->SetPipelineState(g_trianglePipelineState.Get());
    g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_commandList->IASetVertexBuffers(0, 1, &g_cubeBufferView);
    UpdateSceneConstants(DirectX::XMMatrixTranslation(static_cast<float>(g_cursorX), static_cast<float>(g_cursorY), static_cast<float>(g_cursorZ)));
    g_commandList->DrawInstanced(36, 1, 0, 0);

    const auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        g_renderTargets[g_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    g_commandList->ResourceBarrier(1, &toPresent);

    ThrowIfFailed(g_commandList->Close());
}

void Render()
{
    UpdateQuadVertexBuffer();
    PopulateCommandList();

    ID3D12CommandList* commandLists[] = {g_commandList.Get()};
    g_commandQueue->ExecuteCommandLists(1, commandLists);

    ThrowIfFailed(g_swapChain->Present(1, 0));
    WaitForGpu();
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        switch (static_cast<unsigned char>(wParam))
        {
        case VK_ESCAPE:
            PostQuitMessage(0);
            return 0;
        case 'W':
            g_cursorZ -= 1;
            return 0;
        case 'S':
            g_cursorZ += 1;
            return 0;
        case 'A':
            g_cursorX -= 1;
            return 0;
        case 'D':
            g_cursorX += 1;
            return 0;
        case 'Q':
            g_cursorY += 1;
            return 0;
        case 'Z':
            g_cursorY -= 1;
            return 0;
        case VK_SPACE:
            AddQuadPoint();
            return 0;
        case 'R':
            SetCurrentQuadColor(1.0f, 0.0f, 0.0f);
            return 0;
        case 'G':
            SetCurrentQuadColor(0.0f, 1.0f, 0.0f);
            return 0;
        case 'B':
            SetCurrentQuadColor(0.0f, 0.0f, 1.0f);
            return 0;
        case 'Y':
            SetCurrentQuadColor(1.0f, 1.0f, 0.0f);
            return 0;
        default:
            break;
        }
        break;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

void InitializeWindow(HINSTANCE instanceHandle)
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instanceHandle;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = L"D3D12ModelingToolWindow";
    ThrowIfFailed(RegisterClassExW(&windowClass) ? S_OK : HRESULT_FROM_WIN32(GetLastError()));

    RECT windowRect = {0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    g_hwnd = CreateWindowW(
        windowClass.lpszClassName,
        L"D3D12 3D Modeling Tool",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instanceHandle,
        nullptr);

    ThrowIfFailed(g_hwnd ? S_OK : HRESULT_FROM_WIN32(GetLastError()));

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);
}

void InitializeDeviceAndSwapChain()
{
    UINT factoryFlags = 0;
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    ComPtr<IDXGIFactory6> factory;
    ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0; factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        {
            continue;
        }

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device))))
        {
            break;
        }
    }

    if (!g_device)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
        ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)));
    }

    const D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    ThrowIfFailed(g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = kWindowWidth;
    swapChainDesc.Height = kWindowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = kFrameCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        g_commandQueue.Get(),
        g_hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain));
    ThrowIfFailed(factory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain.As(&g_swapChain));
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    ThrowIfFailed(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator)));
    ThrowIfFailed(g_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&g_commandList)));
    ThrowIfFailed(g_commandList->Close());

    ThrowIfFailed(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)));
    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    ThrowIfFailed(g_fenceEvent ? S_OK : HRESULT_FROM_WIN32(GetLastError()));
}

void CreateRenderTargetViews()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = kFrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap)));
    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; ++i)
    {
        ThrowIfFailed(g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i])));
        g_device->CreateRenderTargetView(g_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += g_rtvDescriptorSize;
    }
}

void CreateDepthBuffer()
{
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(g_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&g_dsvHeap)));

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Alignment = 0;
    depthDesc.Width = kWindowWidth;
    depthDesc.Height = kWindowHeight;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    const D3D12_CLEAR_VALUE clearValue = {
        DXGI_FORMAT_D32_FLOAT,
        {1.0f, 0}};

    const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(g_device->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&g_depthStencil)));

    const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {
        DXGI_FORMAT_D32_FLOAT,
        D3D12_DSV_DIMENSION_TEXTURE2D,
        D3D12_DSV_FLAG_NONE};
    g_device->CreateDepthStencilView(g_depthStencil.Get(), &dsvDesc, g_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void CreateConstantBuffer()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(g_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&g_cbvHeap)));

    const UINT constantBufferSize = (sizeof(SceneConstants) + 255u) & ~255u;
    const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);

    const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(g_device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_constantBuffer)));

    const D3D12_RANGE readRange = {0, 0};
    void* mappedData = nullptr;
    ThrowIfFailed(g_constantBuffer->Map(0, &readRange, &mappedData));
    g_constantBufferData = static_cast<SceneConstants*>(mappedData);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = g_constantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = constantBufferSize;
    g_device->CreateConstantBufferView(&cbvDesc, g_cbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void CreateRootSignatureAndPipeline()
{
    D3D12_DESCRIPTOR_RANGE descriptorRange = {};
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.RegisterSpace = 0;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameter.DescriptorTable.NumDescriptorRanges = 1;
    rootParameter.DescriptorTable.pDescriptorRanges = &descriptorRange;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.pParameters = &rootParameter;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serializedRootSignature;
    ComPtr<ID3DBlob> errorBlob;
    ThrowIfFailed(D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRootSignature,
        &errorBlob));
    ThrowIfFailed(g_device->CreateRootSignature(
        0,
        serializedRootSignature->GetBufferPointer(),
        serializedRootSignature->GetBufferSize(),
        IID_PPV_ARGS(&g_rootSignature)));

    const char* shaderSource = R"(
cbuffer SceneConstants : register(b0)
{
    float4x4 worldViewProj;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), worldViewProj);
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
)";

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ThrowIfFailed(D3DCompile(
        shaderSource,
        std::strlen(shaderSource),
        nullptr,
        nullptr,
        nullptr,
        "VSMain",
        "vs_5_0",
        compileFlags,
        0,
        &vertexShader,
        &errorBlob));
    ThrowIfFailed(D3DCompile(
        shaderSource,
        std::strlen(shaderSource),
        nullptr,
        nullptr,
        nullptr,
        "PSMain",
        "ps_5_0",
        compileFlags,
        0,
        &pixelShader,
        &errorBlob));

    const D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = g_rootSignature.Get();
    psoDesc.InputLayout = {inputElements, _countof(inputElements)};
    psoDesc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
    psoDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
    renderTargetBlendDesc.BlendEnable = FALSE;
    renderTargetBlendDesc.LogicOpEnable = FALSE;
    renderTargetBlendDesc.SrcBlend = D3D12_BLEND_ONE;
    renderTargetBlendDesc.DestBlend = D3D12_BLEND_ZERO;
    renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    renderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    for (auto& target : psoDesc.BlendState.RenderTarget)
    {
        target = renderTargetBlendDesc;
    }
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    ThrowIfFailed(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_trianglePipelineState)));

    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    ThrowIfFailed(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_linePipelineState)));
}

void CreateGeometryBuffers()
{
    Vertex gridVertices[40 * 2] = {};
    for (int i = 0; i < 40; ++i)
    {
        const XMFLOAT4 white(1.0f, 1.0f, 1.0f, 1.0f);
        if (i < 20)
        {
            gridVertices[i * 2] = {XMFLOAT3(0.0f, -0.1f, static_cast<float>(i)), white};
            gridVertices[i * 2 + 1] = {XMFLOAT3(19.0f, -0.1f, static_cast<float>(i)), white};
        }
        else
        {
            const float x = static_cast<float>(i - 20);
            gridVertices[i * 2] = {XMFLOAT3(x, -0.1f, 0.0f), white};
            gridVertices[i * 2 + 1] = {XMFLOAT3(x, -0.1f, 19.0f), white};
        }
    }
    CreateUploadVertexBuffer(gridVertices, _countof(gridVertices), g_gridVertexBuffer, g_gridBufferView);

    constexpr float halfCube = 0.2f;
    const XMFLOAT4 cubeColor(1.0f, 1.0f, 1.0f, 1.0f);
    const Vertex cubeVertices[] = {
        {{-halfCube, -halfCube, halfCube}, cubeColor}, {{halfCube, -halfCube, halfCube}, cubeColor}, {{halfCube, halfCube, halfCube}, cubeColor},
        {{-halfCube, -halfCube, halfCube}, cubeColor}, {{halfCube, halfCube, halfCube}, cubeColor}, {{-halfCube, halfCube, halfCube}, cubeColor},
        {{halfCube, -halfCube, -halfCube}, cubeColor}, {{-halfCube, -halfCube, -halfCube}, cubeColor}, {{-halfCube, halfCube, -halfCube}, cubeColor},
        {{halfCube, -halfCube, -halfCube}, cubeColor}, {{-halfCube, halfCube, -halfCube}, cubeColor}, {{halfCube, halfCube, -halfCube}, cubeColor},
        {{-halfCube, halfCube, halfCube}, cubeColor}, {{halfCube, halfCube, halfCube}, cubeColor}, {{halfCube, halfCube, -halfCube}, cubeColor},
        {{-halfCube, halfCube, halfCube}, cubeColor}, {{halfCube, halfCube, -halfCube}, cubeColor}, {{-halfCube, halfCube, -halfCube}, cubeColor},
        {{-halfCube, -halfCube, -halfCube}, cubeColor}, {{halfCube, -halfCube, -halfCube}, cubeColor}, {{halfCube, -halfCube, halfCube}, cubeColor},
        {{-halfCube, -halfCube, -halfCube}, cubeColor}, {{halfCube, -halfCube, halfCube}, cubeColor}, {{-halfCube, -halfCube, halfCube}, cubeColor},
        {{halfCube, -halfCube, halfCube}, cubeColor}, {{halfCube, -halfCube, -halfCube}, cubeColor}, {{halfCube, halfCube, -halfCube}, cubeColor},
        {{halfCube, -halfCube, halfCube}, cubeColor}, {{halfCube, halfCube, -halfCube}, cubeColor}, {{halfCube, halfCube, halfCube}, cubeColor},
        {{-halfCube, -halfCube, -halfCube}, cubeColor}, {{-halfCube, -halfCube, halfCube}, cubeColor}, {{-halfCube, halfCube, halfCube}, cubeColor},
        {{-halfCube, -halfCube, -halfCube}, cubeColor}, {{-halfCube, halfCube, halfCube}, cubeColor}, {{-halfCube, halfCube, -halfCube}, cubeColor}};
    CreateUploadVertexBuffer(cubeVertices, _countof(cubeVertices), g_cubeVertexBuffer, g_cubeBufferView);

    const UINT quadBufferSize = static_cast<UINT>(kMaxDrawableQuads * 6 * sizeof(Vertex));
    const auto quadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(quadBufferSize);
    const CD3DX12_HEAP_PROPERTIES quadUploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(g_device->CreateCommittedResource(
        &quadUploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &quadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_quadVertexBuffer)));

    const D3D12_RANGE readRange = {0, 0};
    void* mappedData = nullptr;
    ThrowIfFailed(g_quadVertexBuffer->Map(0, &readRange, &mappedData));
    g_quadVertexData = static_cast<Vertex*>(mappedData);
    std::memset(g_quadVertexData, 0, quadBufferSize);

    g_quadBufferView.BufferLocation = g_quadVertexBuffer->GetGPUVirtualAddress();
    g_quadBufferView.SizeInBytes = quadBufferSize;
    g_quadBufferView.StrideInBytes = sizeof(Vertex);
}

void InitializeApp(HINSTANCE instanceHandle)
{
    InitializeWindow(instanceHandle);
    InitializeDeviceAndSwapChain();
    CreateRenderTargetViews();
    CreateDepthBuffer();
    CreateConstantBuffer();
    CreateRootSignatureAndPipeline();
    CreateGeometryBuffers();
}

void Cleanup()
{
    if (g_commandQueue && g_fence)
    {
        WaitForGpu();
    }

    if (g_constantBuffer)
    {
        g_constantBuffer->Unmap(0, nullptr);
    }
    if (g_quadVertexBuffer)
    {
        g_quadVertexBuffer->Unmap(0, nullptr);
    }

    if (g_fenceEvent)
    {
        CloseHandle(g_fenceEvent);
        g_fenceEvent = nullptr;
    }
}
} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    try
    {
        InitializeApp(hInstance);

        MSG msg = {};
        while (msg.message != WM_QUIT)
        {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
                Render();
            }
        }

        Cleanup();
        return static_cast<int>(msg.wParam);
    }
    catch (const std::exception&)
    {
        Cleanup();
        MessageBoxA(nullptr, "The D3D12 conversion hit a runtime error.", "D3D12 Modeling Tool", MB_OK | MB_ICONERROR);
        return -1;
    }
}
