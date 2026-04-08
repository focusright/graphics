#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

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
using Microsoft::WRL::ComPtr;
using namespace D3DX12;

namespace
{
constexpr UINT kFrameCount = 2;
constexpr UINT kWindowWidth = 800;
constexpr UINT kWindowHeight = 600;
constexpr int kMaxQuads = 100;
constexpr int kGridVertexCount = 80;
constexpr int kCubeVertexCount = 36;
constexpr int kMaxQuadVertices = (kMaxQuads - 1) * 6;
constexpr UINT kDrawConstantCount = 3;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
};

struct SceneConstants
{
    XMFLOAT4X4 worldViewProj;
};

struct Quads
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
UINT g_rtvDescriptorSize = 0;

ComPtr<ID3D12Resource> g_renderTargets[kFrameCount];
ComPtr<ID3D12Resource> g_depthStencil;
ComPtr<ID3D12Resource> g_constantBuffer;
ComPtr<ID3D12Resource> g_gridVertexBuffer;
ComPtr<ID3D12Resource> g_cubeVertexBuffer;
ComPtr<ID3D12Resource> g_quadVertexBuffer;

ComPtr<ID3D12RootSignature> g_rootSignature;
ComPtr<ID3D12PipelineState> g_trianglePso;
ComPtr<ID3D12PipelineState> g_linePso;

D3D12_VERTEX_BUFFER_VIEW g_gridBufferView = {};
D3D12_VERTEX_BUFFER_VIEW g_cubeBufferView = {};
D3D12_VERTEX_BUFFER_VIEW g_quadBufferView = {};

SceneConstants* g_sceneConstants = nullptr;
Vertex* g_quadVerticesMapped = nullptr;
UINT g_constantBufferStride = 0;

int g_cx = 0;
int g_cy = 0;
int g_cz = 0;
int g_cn = 0;
Quads g_quads[kMaxQuads] = {};

void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::runtime_error("D3D12 failure");
    }
}

D3D12_RESOURCE_BARRIER MakeTransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

void WaitForGpu()
{
    const UINT64 fenceValue = ++g_fenceValue;
    ThrowIfFailed(g_commandQueue->Signal(g_fence.Get(), fenceValue));
    if (g_fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent));
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

void CreateUploadVertexBuffer(const Vertex* vertices, UINT vertexCount, ComPtr<ID3D12Resource>& resource, D3D12_VERTEX_BUFFER_VIEW& view)
{
    const UINT bufferSize = vertexCount * sizeof(Vertex);
    const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
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

void AddQuad()
{
    g_quads[0].state++;
    if (g_quads[0].state > 4)
    {
        g_quads[0].state = 1;
    }

    const int st = g_quads[0].state;
    if (st == 1)
    {
        if (g_quads[0].total >= kMaxQuads - 1)
        {
            return;
        }
        g_quads[0].total++;
        g_cn = g_quads[0].total;
    }

    if (st == 1) { g_quads[g_cn].x1 = g_cx; g_quads[g_cn].y1 = g_cy; g_quads[g_cn].z1 = g_cz; }
    if (st == 1 || st == 2) { g_quads[g_cn].x2 = g_cx; g_quads[g_cn].y2 = g_cy; g_quads[g_cn].z2 = g_cz; }
    if (st == 1 || st == 2 || st == 3) { g_quads[g_cn].x3 = g_cx; g_quads[g_cn].y3 = g_cy; g_quads[g_cn].z3 = g_cz; }
    if (st == 1 || st == 2 || st == 3 || st == 4) { g_quads[g_cn].x4 = g_cx; g_quads[g_cn].y4 = g_cy; g_quads[g_cn].z4 = g_cz; }
}

void UpdateQuadVertexBuffer()
{
    std::memset(g_quadVerticesMapped, 0, kMaxQuadVertices * sizeof(Vertex));

    int vertexIndex = 0;
    for (int i = 1; i < g_quads[0].total + 1; ++i)
    {
        const XMFLOAT4 color(g_quads[i].r, g_quads[i].g, g_quads[i].b, 1.0f);

        g_quadVerticesMapped[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x1), static_cast<float>(g_quads[i].y1), static_cast<float>(g_quads[i].z1)), color};
        g_quadVerticesMapped[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x2), static_cast<float>(g_quads[i].y2), static_cast<float>(g_quads[i].z2)), color};
        g_quadVerticesMapped[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x3), static_cast<float>(g_quads[i].y3), static_cast<float>(g_quads[i].z3)), color};
        g_quadVerticesMapped[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x1), static_cast<float>(g_quads[i].y1), static_cast<float>(g_quads[i].z1)), color};
        g_quadVerticesMapped[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x3), static_cast<float>(g_quads[i].y3), static_cast<float>(g_quads[i].z3)), color};
        g_quadVerticesMapped[vertexIndex++] = {XMFLOAT3(static_cast<float>(g_quads[i].x4), static_cast<float>(g_quads[i].y4), static_cast<float>(g_quads[i].z4)), color};
    }
}

XMMATRIX BuildProjectionMatrix()
{
    return DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XMConvertToRadians(35.0f),
        1.0f,
        0.1f,
        1000.0f);
}

XMMATRIX BuildSceneMatrix()
{
    const XMMATRIX translation = DirectX::XMMatrixTranslation(-13.0f, 0.0f, 45.0f);
    const XMMATRIX rotation = DirectX::XMMatrixRotationAxis(
        DirectX::XMVector3Normalize(DirectX::XMVectorSet(1.0f, 1.0f, 0.0f, 0.0f)),
        DirectX::XMConvertToRadians(40.0f));
    return rotation * translation;
}

void UpdateConstants(UINT drawIndex, const XMMATRIX& world)
{
    const XMMATRIX wvp = world * BuildSceneMatrix() * BuildProjectionMatrix();
    DirectX::XMStoreFloat4x4(&g_sceneConstants[drawIndex].worldViewProj, DirectX::XMMatrixTranspose(wvp));
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
        case VK_ESCAPE: PostQuitMessage(0); return 0;
        case 'W': g_cz -= 1; return 0;
        case 'S': g_cz += 1; return 0;
        case 'A': g_cx -= 1; return 0;
        case 'D': g_cx += 1; return 0;
        case 'Q': g_cy += 1; return 0;
        case 'Z': g_cy -= 1; return 0;
        case VK_SPACE: AddQuad(); return 0;
        case 'R': g_quads[g_cn].r = 1.0f; g_quads[g_cn].g = 0.0f; g_quads[g_cn].b = 0.0f; return 0;
        case 'G': g_quads[g_cn].r = 0.0f; g_quads[g_cn].g = 1.0f; g_quads[g_cn].b = 0.0f; return 0;
        case 'B': g_quads[g_cn].r = 0.0f; g_quads[g_cn].g = 0.0f; g_quads[g_cn].b = 1.0f; return 0;
        case 'Y': g_quads[g_cn].r = 1.0f; g_quads[g_cn].g = 1.0f; g_quads[g_cn].b = 0.0f; return 0;
        default: break;
        }
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void InitializeWindow(HINSTANCE hInstance)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"CodexD3D12ModelingTool";
    ThrowIfFailed(RegisterClassExW(&wc) ? S_OK : HRESULT_FROM_WIN32(GetLastError()));

    RECT rect = {0, 0, static_cast<LONG>(kWindowWidth), static_cast<LONG>(kWindowHeight)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    g_hwnd = CreateWindowW(
        wc.lpszClassName,
        L"D3D12 3D Modeling Tool",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);
    ThrowIfFailed(g_hwnd ? S_OK : HRESULT_FROM_WIN32(GetLastError()));

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);
}

void InitializeD3D12()
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

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT index = 0; factory->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND; ++index)
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
        ComPtr<IDXGIAdapter> warp;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        ThrowIfFailed(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)));
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = kFrameCount;
    swapChainDesc.Width = kWindowWidth;
    swapChainDesc.Height = kWindowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(g_commandQueue.Get(), g_hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1));
    ThrowIfFailed(factory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain1.As(&g_swapChain));
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    ThrowIfFailed(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator)));
    ThrowIfFailed(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_commandList)));
    ThrowIfFailed(g_commandList->Close());

    ThrowIfFailed(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)));
    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    ThrowIfFailed(g_fenceEvent ? S_OK : HRESULT_FROM_WIN32(GetLastError()));
}

void CreateRenderTargets()
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
    depthDesc.Width = kWindowWidth;
    depthDesc.Height = kWindowHeight;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(g_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&g_depthStencil)));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    g_device->CreateDepthStencilView(g_depthStencil.Get(), &dsvDesc, g_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void CreateConstantBuffer()
{
    g_constantBufferStride = (sizeof(SceneConstants) + 255u) & ~255u;
    const UINT bufferSize = g_constantBufferStride * kDrawConstantCount;
    const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(g_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_constantBuffer)));

    const D3D12_RANGE readRange = {0, 0};
    void* mappedData = nullptr;
    ThrowIfFailed(g_constantBuffer->Map(0, &readRange, &mappedData));
    g_sceneConstants = static_cast<SceneConstants*>(mappedData);

    std::memset(g_sceneConstants, 0, bufferSize);
}

void CreatePipeline()
{
    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameter.Descriptor.ShaderRegister = 0;
    rootParameter.Descriptor.RegisterSpace = 0;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.pParameters = &rootParameter;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errorBlob));
    ThrowIfFailed(g_device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&g_rootSignature)));

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
    ThrowIfFailed(D3DCompile(shaderSource, std::strlen(shaderSource), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errorBlob));
    ThrowIfFailed(D3DCompile(shaderSource, std::strlen(shaderSource), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorBlob));

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = g_rootSignature.Get();
    psoDesc.InputLayout = {inputLayout, _countof(inputLayout)};
    psoDesc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
    psoDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC blendDesc = {};
    blendDesc.BlendEnable = FALSE;
    blendDesc.LogicOpEnable = FALSE;
    blendDesc.SrcBlend = D3D12_BLEND_ONE;
    blendDesc.DestBlend = D3D12_BLEND_ZERO;
    blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    for (auto& target : psoDesc.BlendState.RenderTarget)
    {
        target = blendDesc;
    }
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_trianglePso)));

    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    ThrowIfFailed(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_linePso)));

}

void CreateGeometry()
{
    Vertex gridVertices[kGridVertexCount] = {};
    for (int i = 0; i < 40; ++i)
    {
        const XMFLOAT4 white(1.0f, 1.0f, 1.0f, 1.0f);
        if (i < 20)
        {
            gridVertices[i * 2] = {XMFLOAT3(0.0f, -0.1f, static_cast<float>(i)), white};
            gridVertices[i * 2 + 1] = {XMFLOAT3(19.0f, -0.1f, static_cast<float>(i)), white};
        }
        else if (i > 20)
        {
            gridVertices[i * 2] = {XMFLOAT3(static_cast<float>(i - 20), -0.1f, 0.0f), white};
            gridVertices[i * 2 + 1] = {XMFLOAT3(static_cast<float>(i - 20), -0.1f, 19.0f), white};
        }
        else
        {
            gridVertices[i * 2] = {XMFLOAT3(0.0f, -0.1f, 0.0f), white};
            gridVertices[i * 2 + 1] = {XMFLOAT3(19.0f, -0.1f, 0.0f), white};
        }
    }
    CreateUploadVertexBuffer(gridVertices, kGridVertexCount, g_gridVertexBuffer, g_gridBufferView);

    constexpr float s = 0.2f;
    const XMFLOAT4 white(1.0f, 1.0f, 1.0f, 1.0f);
    const Vertex cubeVertices[kCubeVertexCount] =
    {
        {{-s, -s,  s}, white}, {{ s, -s,  s}, white}, {{ s,  s,  s}, white},
        {{-s, -s,  s}, white}, {{ s,  s,  s}, white}, {{-s,  s,  s}, white},
        {{ s, -s, -s}, white}, {{-s, -s, -s}, white}, {{-s,  s, -s}, white},
        {{ s, -s, -s}, white}, {{-s,  s, -s}, white}, {{ s,  s, -s}, white},
        {{-s,  s,  s}, white}, {{ s,  s,  s}, white}, {{ s,  s, -s}, white},
        {{-s,  s,  s}, white}, {{ s,  s, -s}, white}, {{-s,  s, -s}, white},
        {{-s, -s, -s}, white}, {{ s, -s, -s}, white}, {{ s, -s,  s}, white},
        {{-s, -s, -s}, white}, {{ s, -s,  s}, white}, {{-s, -s,  s}, white},
        {{ s, -s,  s}, white}, {{ s, -s, -s}, white}, {{ s,  s, -s}, white},
        {{ s, -s,  s}, white}, {{ s,  s, -s}, white}, {{ s,  s,  s}, white},
        {{-s, -s, -s}, white}, {{-s, -s,  s}, white}, {{-s,  s,  s}, white},
        {{-s, -s, -s}, white}, {{-s,  s,  s}, white}, {{-s,  s, -s}, white}
    };
    CreateUploadVertexBuffer(cubeVertices, kCubeVertexCount, g_cubeVertexBuffer, g_cubeBufferView);

    const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(kMaxQuadVertices * sizeof(Vertex));
    const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(g_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_quadVertexBuffer)));

    const D3D12_RANGE readRange = {0, 0};
    void* mappedData = nullptr;
    ThrowIfFailed(g_quadVertexBuffer->Map(0, &readRange, &mappedData));
    g_quadVerticesMapped = static_cast<Vertex*>(mappedData);
    std::memset(g_quadVerticesMapped, 0, kMaxQuadVertices * sizeof(Vertex));

    g_quadBufferView.BufferLocation = g_quadVertexBuffer->GetGPUVirtualAddress();
    g_quadBufferView.SizeInBytes = kMaxQuadVertices * sizeof(Vertex);
    g_quadBufferView.StrideInBytes = sizeof(Vertex);
}

void RecordAndSubmitFrame()
{
    UpdateQuadVertexBuffer();

    ThrowIfFailed(g_commandAllocator->Reset());
    ThrowIfFailed(g_commandList->Reset(g_commandAllocator.Get(), nullptr));

    g_commandList->SetGraphicsRootSignature(g_rootSignature.Get());

    D3D12_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(kWindowWidth), static_cast<float>(kWindowHeight), 0.0f, 1.0f};
    D3D12_RECT scissorRect = {0, 0, static_cast<LONG>(kWindowWidth), static_cast<LONG>(kWindowHeight)};
    g_commandList->RSSetViewports(1, &viewport);
    g_commandList->RSSetScissorRects(1, &scissorRect);

    const auto toRenderTarget = MakeTransitionBarrier(g_renderTargets[g_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    g_commandList->ResourceBarrier(1, &toRenderTarget);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(g_frameIndex) * g_rtvDescriptorSize;
    const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = g_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    g_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = {0.1f, 0.1f, 0.1f, 1.0f};
    g_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    g_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    g_commandList->SetPipelineState(g_linePso.Get());
    g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    g_commandList->IASetVertexBuffers(0, 1, &g_gridBufferView);
    UpdateConstants(0, DirectX::XMMatrixIdentity());
    g_commandList->SetGraphicsRootConstantBufferView(0, g_constantBuffer->GetGPUVirtualAddress() + static_cast<UINT64>(g_constantBufferStride) * 0);
    g_commandList->DrawInstanced(kGridVertexCount, 1, 0, 0);

    if (g_quads[0].total > 0)
    {
        g_commandList->SetPipelineState(g_trianglePso.Get());
        g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_commandList->IASetVertexBuffers(0, 1, &g_quadBufferView);
        UpdateConstants(1, DirectX::XMMatrixIdentity());
        g_commandList->SetGraphicsRootConstantBufferView(0, g_constantBuffer->GetGPUVirtualAddress() + static_cast<UINT64>(g_constantBufferStride) * 1);
        g_commandList->DrawInstanced(g_quads[0].total * 6, 1, 0, 0);
    }

    g_commandList->SetPipelineState(g_trianglePso.Get());
    g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_commandList->IASetVertexBuffers(0, 1, &g_cubeBufferView);
    UpdateConstants(2, DirectX::XMMatrixTranslation(static_cast<float>(g_cx), static_cast<float>(g_cy), static_cast<float>(g_cz)));
    g_commandList->SetGraphicsRootConstantBufferView(0, g_constantBuffer->GetGPUVirtualAddress() + static_cast<UINT64>(g_constantBufferStride) * 2);
    g_commandList->DrawInstanced(kCubeVertexCount, 1, 0, 0);

    const auto toPresent = MakeTransitionBarrier(g_renderTargets[g_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    g_commandList->ResourceBarrier(1, &toPresent);

    ThrowIfFailed(g_commandList->Close());

    ID3D12CommandList* commandLists[] = {g_commandList.Get()};
    g_commandQueue->ExecuteCommandLists(1, commandLists);
    ThrowIfFailed(g_swapChain->Present(1, 0));
    WaitForGpu();
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();
}

void Cleanup()
{
    if (g_commandQueue && g_fence)
    {
        WaitForGpu();
    }
    if (g_quadVertexBuffer) { g_quadVertexBuffer->Unmap(0, nullptr); }
    if (g_constantBuffer) { g_constantBuffer->Unmap(0, nullptr); }
    if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = nullptr; }
}

void InitializeApp(HINSTANCE hInstance)
{
    InitializeWindow(hInstance);
    InitializeD3D12();
    CreateRenderTargets();
    CreateDepthBuffer();
    CreateConstantBuffer();
    CreatePipeline();
    CreateGeometry();
}
}

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
                RecordAndSubmitFrame();
            }
        }

        Cleanup();
        return static_cast<int>(msg.wParam);
    }
    catch (const std::exception&)
    {
        Cleanup();
        MessageBoxA(nullptr, "The fresh D3D12 conversion failed.", "D3D12 Modeling Tool", MB_OK | MB_ICONERROR);
        return -1;
    }
}
