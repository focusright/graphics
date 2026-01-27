#include <windows.h>
#include <windowsx.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cmath>

#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace D3DX12;

// Window dimensions
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

// Global variables
HWND g_hwnd = nullptr;
ComPtr<ID3D12Device> g_device;
ComPtr<ID3D12CommandQueue> g_commandQueue;
ComPtr<ID3D12CommandAllocator> g_commandAllocator;
ComPtr<ID3D12GraphicsCommandList> g_commandList;
ComPtr<ID3D12RootSignature> g_rootSignature;
ComPtr<ID3D12PipelineState> g_pipelineStateSolid;
ComPtr<ID3D12PipelineState> g_pipelineStateLine;
ComPtr<ID3D12Resource> g_vertexBufferGrid;
ComPtr<ID3D12Resource> g_vertexBufferCube;
ComPtr<ID3D12Resource> g_vertexBufferQuads;
ComPtr<ID3D12Resource> g_constantBuffer;
ComPtr<ID3D12DescriptorHeap> g_rtvHeap;
ComPtr<ID3D12DescriptorHeap> g_dsvHeap;
ComPtr<ID3D12DescriptorHeap> g_cbvHeap;
ComPtr<ID3D12Resource> g_renderTargets[2];
ComPtr<ID3D12Resource> g_depthStencil;
UINT g_rtvDescriptorSize;
UINT g_frameIndex;
ComPtr<ID3D12Fence> g_fence;
UINT64 g_fenceValue;
HANDLE g_fenceEvent;
ComPtr<IDXGISwapChain3> g_swapChain;

// Vertex structure
struct Vertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
};

// Constant buffer structure
struct ConstantBuffer {
    XMMATRIX viewProj;
};

// Quad structure (matching original)
struct Quads {
    int x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;
    float r, g, b;
    int state;
    int total;
};

// Application state
int g_cx = 0, g_cy = 0, g_cz = 0;
int g_cn = 0;
Quads g_quads[100];
bool g_keys[256] = { false };

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitializeWindow(HINSTANCE hInstance);
void InitializeDirect3D();
void CreatePipelineStates();
void CreateVertexBuffers();
void CreateConstantBuffer();
void CreateDepthStencil();
void PopulateCommandList();
void WaitForGpu();
void MoveToNextFrame();
void UpdateConstantBuffer();
void UpdateQuadBuffer();
void AddQuad();
void DrawGrid();
void DrawCube();
void DrawQuads();
void HandleKeyboard();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitializeWindow(hInstance);
    InitializeDirect3D();
    CreatePipelineStates();
    CreateVertexBuffers();
    CreateConstantBuffer();
    CreateDepthStencil();

    // Initialize quads
    g_quads[0].state = 0;
    g_quads[0].total = 0;

    // Main message loop
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            HandleKeyboard();
            UpdateConstantBuffer();
            UpdateQuadBuffer();
            PopulateCommandList();
            WaitForGpu();
            g_swapChain->Present(1, 0);
            MoveToNextFrame();
        }
    }

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                PostQuitMessage(0);
                return 0;
            }
            if (wParam < 256) {
                g_keys[wParam] = true;
            }
            return 0;
        case WM_KEYUP:
            if (wParam < 256) {
                g_keys[wParam] = false;
            }
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void InitializeWindow(HINSTANCE hInstance) {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"D3D12ModelingTool";
    RegisterClassEx(&wc);

    RECT windowRect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    g_hwnd = CreateWindow(
        L"D3D12ModelingTool",
        L"D3D12 3D Modeling Tool",
        WS_OVERLAPPEDWINDOW,
        x, y,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    ShowWindow(g_hwnd, SW_SHOW);
}

void InitializeDirect3D() {
    UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    ComPtr<IDXGIFactory6> factory;
    CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));

    ComPtr<IDXGIAdapter1> hardwareAdapter;
    for (UINT adapterIndex = 0; SUCCEEDED(factory->EnumAdapters1(adapterIndex, &hardwareAdapter)); ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc;
        hardwareAdapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)))) break;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = WINDOW_WIDTH;
    swapChainDesc.Height = WINDOW_HEIGHT;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    factory->CreateSwapChainForHwnd(
        g_commandQueue.Get(),
        g_hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );

    swapChain1.As(&g_swapChain);
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 2;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap));
    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < 2; i++) {
        g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i]));
        g_device->CreateRenderTargetView(g_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += g_rtvDescriptorSize;
    }

    g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator));
    g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_commandList));
    g_commandList->Close();

    g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
    g_fenceValue = 1;
    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // Create CBV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    g_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&g_cbvHeap));
}

void CreatePipelineStates() {
    // Create root signature with constant buffer view
    D3D12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[1];
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = ranges;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(1, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        if (error) {
            OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        }
        return;
    }
    g_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&g_rootSignature));

    // Create shaders
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    const char* vertexShaderSource = R"(
        struct PSInput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };

        cbuffer ConstantBuffer : register(b0) {
            float4x4 viewProj;
        };

        PSInput VSMain(float3 position : POSITION, float4 color : COLOR) {
            PSInput result;
            float4 pos = float4(position, 1.0f);
            result.position = mul(pos, viewProj);
            result.color = color;
            return result;
        }
    )";

    const char* pixelShaderSource = R"(
        struct PSInput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };

        float4 PSMain(PSInput input) : SV_TARGET {
            return input.color;
        }
    )";

    if (FAILED(D3DCompile(vertexShaderSource, strlen(vertexShaderSource), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &error))) {
        if (error) {
            OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        }
        return;
    }

    if (FAILED(D3DCompile(pixelShaderSource, strlen(pixelShaderSource), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &error))) {
        if (error) {
            OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        }
        return;
    }

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Solid pipeline state
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = g_rootSignature.Get();
    psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    if (FAILED(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelineStateSolid)))) {
        return;
    }

    // Line pipeline state
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    if (FAILED(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelineStateLine)))) {
        return;
    }
}

void CreateVertexBuffers() {
    // Create grid vertex buffer (40 lines)
    const int gridLineCount = 40;
    const int gridVertexCount = gridLineCount * 2;
    Vertex gridVertices[gridVertexCount];
    
    for (int i = 0; i < gridLineCount; i++) {
        XMFLOAT4 color(1.0f, 1.0f, 1.0f, 1.0f);
        if (i < 20) {
            // Horizontal lines
            gridVertices[i * 2] = { XMFLOAT3(0.0f, -0.1f, static_cast<float>(i)), color };
            gridVertices[i * 2 + 1] = { XMFLOAT3(19.0f, -0.1f, static_cast<float>(i)), color };
        } else {
            // Vertical lines
            int offset = i - 20;
            gridVertices[i * 2] = { XMFLOAT3(static_cast<float>(offset), -0.1f, 0.0f), color };
            gridVertices[i * 2 + 1] = { XMFLOAT3(static_cast<float>(offset), -0.1f, 19.0f), color };
        }
    }

    const UINT gridBufferSize = sizeof(gridVertices);
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(gridBufferSize);
    g_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_vertexBufferGrid)
    );

    UINT8* pVertexDataBegin;
    D3D12_RANGE readRange = { 0, 0 };
    g_vertexBufferGrid->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    memcpy(pVertexDataBegin, gridVertices, sizeof(gridVertices));
    g_vertexBufferGrid->Unmap(0, nullptr);

    // Create cube vertex buffer (12 triangles = 36 vertices)
    const float cubeSize = 0.2f; // 0.4 / 2
    Vertex cubeVertices[36];
    XMFLOAT4 cubeColor(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Front face
    cubeVertices[0] = { XMFLOAT3(-cubeSize, -cubeSize, cubeSize), cubeColor };
    cubeVertices[1] = { XMFLOAT3(cubeSize, -cubeSize, cubeSize), cubeColor };
    cubeVertices[2] = { XMFLOAT3(cubeSize, cubeSize, cubeSize), cubeColor };
    cubeVertices[3] = { XMFLOAT3(-cubeSize, -cubeSize, cubeSize), cubeColor };
    cubeVertices[4] = { XMFLOAT3(cubeSize, cubeSize, cubeSize), cubeColor };
    cubeVertices[5] = { XMFLOAT3(-cubeSize, cubeSize, cubeSize), cubeColor };
    
    // Back face
    cubeVertices[6] = { XMFLOAT3(cubeSize, -cubeSize, -cubeSize), cubeColor };
    cubeVertices[7] = { XMFLOAT3(-cubeSize, -cubeSize, -cubeSize), cubeColor };
    cubeVertices[8] = { XMFLOAT3(-cubeSize, cubeSize, -cubeSize), cubeColor };
    cubeVertices[9] = { XMFLOAT3(cubeSize, -cubeSize, -cubeSize), cubeColor };
    cubeVertices[10] = { XMFLOAT3(-cubeSize, cubeSize, -cubeSize), cubeColor };
    cubeVertices[11] = { XMFLOAT3(cubeSize, cubeSize, -cubeSize), cubeColor };
    
    // Top face
    cubeVertices[12] = { XMFLOAT3(-cubeSize, cubeSize, cubeSize), cubeColor };
    cubeVertices[13] = { XMFLOAT3(cubeSize, cubeSize, cubeSize), cubeColor };
    cubeVertices[14] = { XMFLOAT3(cubeSize, cubeSize, -cubeSize), cubeColor };
    cubeVertices[15] = { XMFLOAT3(-cubeSize, cubeSize, cubeSize), cubeColor };
    cubeVertices[16] = { XMFLOAT3(cubeSize, cubeSize, -cubeSize), cubeColor };
    cubeVertices[17] = { XMFLOAT3(-cubeSize, cubeSize, -cubeSize), cubeColor };
    
    // Bottom face
    cubeVertices[18] = { XMFLOAT3(-cubeSize, -cubeSize, -cubeSize), cubeColor };
    cubeVertices[19] = { XMFLOAT3(cubeSize, -cubeSize, -cubeSize), cubeColor };
    cubeVertices[20] = { XMFLOAT3(cubeSize, -cubeSize, cubeSize), cubeColor };
    cubeVertices[21] = { XMFLOAT3(-cubeSize, -cubeSize, -cubeSize), cubeColor };
    cubeVertices[22] = { XMFLOAT3(cubeSize, -cubeSize, cubeSize), cubeColor };
    cubeVertices[23] = { XMFLOAT3(-cubeSize, -cubeSize, cubeSize), cubeColor };
    
    // Right face
    cubeVertices[24] = { XMFLOAT3(cubeSize, -cubeSize, cubeSize), cubeColor };
    cubeVertices[25] = { XMFLOAT3(cubeSize, -cubeSize, -cubeSize), cubeColor };
    cubeVertices[26] = { XMFLOAT3(cubeSize, cubeSize, -cubeSize), cubeColor };
    cubeVertices[27] = { XMFLOAT3(cubeSize, -cubeSize, cubeSize), cubeColor };
    cubeVertices[28] = { XMFLOAT3(cubeSize, cubeSize, -cubeSize), cubeColor };
    cubeVertices[29] = { XMFLOAT3(cubeSize, cubeSize, cubeSize), cubeColor };
    
    // Left face
    cubeVertices[30] = { XMFLOAT3(-cubeSize, -cubeSize, -cubeSize), cubeColor };
    cubeVertices[31] = { XMFLOAT3(-cubeSize, -cubeSize, cubeSize), cubeColor };
    cubeVertices[32] = { XMFLOAT3(-cubeSize, cubeSize, cubeSize), cubeColor };
    cubeVertices[33] = { XMFLOAT3(-cubeSize, -cubeSize, -cubeSize), cubeColor };
    cubeVertices[34] = { XMFLOAT3(-cubeSize, cubeSize, cubeSize), cubeColor };
    cubeVertices[35] = { XMFLOAT3(-cubeSize, cubeSize, -cubeSize), cubeColor };

    const UINT cubeBufferSize = sizeof(cubeVertices);
    resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(cubeBufferSize);
    g_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_vertexBufferCube)
    );

    g_vertexBufferCube->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    memcpy(pVertexDataBegin, cubeVertices, sizeof(cubeVertices));
    g_vertexBufferCube->Unmap(0, nullptr);

    // Create quad vertex buffer (will be updated dynamically, max 100 quads = 400 vertices = 600 triangle indices)
    const UINT maxQuadVertices = 100 * 6; // Each quad = 2 triangles = 6 vertices
    const UINT quadBufferSize = maxQuadVertices * sizeof(Vertex);
    resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(quadBufferSize);
    g_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_vertexBufferQuads)
    );
}

void CreateConstantBuffer() {
    const UINT constantBufferSize = (sizeof(ConstantBuffer) + 255) & ~255; // Align to 256 bytes
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
    g_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_constantBuffer)
    );

    // Create CBV
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = g_constantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = constantBufferSize;
    D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = g_cbvHeap->GetCPUDescriptorHandleForHeapStart();
    g_device->CreateConstantBufferView(&cbvDesc, cbvHandle);
}

void CreateDepthStencil() {
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    g_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&g_dsvHeap));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_RESOURCE_DESC depthStencilDesc = {};
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = WINDOW_WIDTH;
    depthStencilDesc.Height = WINDOW_HEIGHT;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    g_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(&g_depthStencil)
    );

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = g_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    g_device->CreateDepthStencilView(g_depthStencil.Get(), &dsvDesc, dsvHandle);
}

void PopulateCommandList() {
    g_commandAllocator->Reset();
    g_commandList->Reset(g_commandAllocator.Get(), g_pipelineStateSolid.Get());

    g_commandList->SetGraphicsRootSignature(g_rootSignature.Get());
    
    D3D12_VIEWPORT viewport{
        0.0f, 0.0f,
        static_cast<float>(WINDOW_WIDTH),
        static_cast<float>(WINDOW_HEIGHT),
        0.0f, 1.0f
    };
    D3D12_RECT scissorRect{ 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    g_commandList->RSSetViewports(1, &viewport);
    g_commandList->RSSetScissorRects(1, &scissorRect);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        g_renderTargets[g_frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    g_commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += g_frameIndex * g_rtvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = g_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    g_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    g_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    g_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Calculate view and projection matrices
    XMMATRIX view = XMMatrixTranslation(-13.0f, 0.0f, -45.0f) * XMMatrixRotationRollPitchYaw(XM_PI / 9.0f, XM_PI / 9.0f, 0.0f);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(35.0f), static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT, 0.1f, 1000.0f);
    XMMATRIX viewProj = XMMatrixTranspose(view * proj);
    
    // Update constant buffer
    ConstantBuffer cb;
    cb.viewProj = viewProj;
    UINT8* pCbDataBegin;
    D3D12_RANGE readRange = { 0, 0 };
    g_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pCbDataBegin));
    memcpy(pCbDataBegin, &cb, sizeof(cb));
    g_constantBuffer->Unmap(0, nullptr);
    
    // Set descriptor table
    D3D12_GPU_DESCRIPTOR_HANDLE cbvHandle = g_cbvHeap->GetGPUDescriptorHandleForHeapStart();
    g_commandList->SetGraphicsRootDescriptorTable(0, cbvHandle);

    // Draw grid (lines)
    g_commandList->SetPipelineState(g_pipelineStateLine.Get());
    g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    D3D12_VERTEX_BUFFER_VIEW gridBufferView{
        g_vertexBufferGrid->GetGPUVirtualAddress(),
        40 * 2 * sizeof(Vertex),
        sizeof(Vertex)
    };
    g_commandList->IASetVertexBuffers(0, 1, &gridBufferView);
    g_commandList->DrawInstanced(40 * 2, 1, 0, 0);

    // Draw quads (triangles)
    g_commandList->SetPipelineState(g_pipelineStateSolid.Get());
    g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if (g_quads[0].total > 0) {
        D3D12_VERTEX_BUFFER_VIEW quadBufferView{
            g_vertexBufferQuads->GetGPUVirtualAddress(),
            g_quads[0].total * 6 * sizeof(Vertex),
            sizeof(Vertex)
        };
        g_commandList->IASetVertexBuffers(0, 1, &quadBufferView);
        g_commandList->DrawInstanced(g_quads[0].total * 6, 1, 0, 0);
    }

    // Draw cube (triangles)
    XMMATRIX cubeWorld = XMMatrixTranslation(static_cast<float>(g_cx), static_cast<float>(g_cy), static_cast<float>(g_cz));
    XMMATRIX cubeViewProj = XMMatrixTranspose(cubeWorld * view * proj);
    
    // Update constant buffer for cube
    ConstantBuffer cubeCb;
    cubeCb.viewProj = cubeViewProj;
    g_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pCbDataBegin));
    memcpy(pCbDataBegin, &cubeCb, sizeof(cubeCb));
    g_constantBuffer->Unmap(0, nullptr);
    
    D3D12_VERTEX_BUFFER_VIEW cubeBufferView{
        g_vertexBufferCube->GetGPUVirtualAddress(),
        36 * sizeof(Vertex),
        sizeof(Vertex)
    };
    g_commandList->IASetVertexBuffers(0, 1, &cubeBufferView);
    g_commandList->DrawInstanced(36, 1, 0, 0);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        g_renderTargets[g_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    g_commandList->ResourceBarrier(1, &barrier);

    g_commandList->Close();

    ID3D12CommandList* ppCommandLists[] = { g_commandList.Get() };
    g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

void WaitForGpu() {
    const UINT64 fence = g_fenceValue;
    g_commandQueue->Signal(g_fence.Get(), fence);
    g_fenceValue++;

    if (g_fence->GetCompletedValue() < fence) {
        g_fence->SetEventOnCompletion(fence, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

void MoveToNextFrame() {
    const UINT64 currentFenceValue = g_fenceValue;
    g_commandQueue->Signal(g_fence.Get(), currentFenceValue);
    g_fenceValue++;

    if (g_fence->GetCompletedValue() < currentFenceValue) {
        g_fence->SetEventOnCompletion(currentFenceValue, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }

    g_frameIndex = 1 - g_frameIndex;
}

void UpdateConstantBuffer() {
    // Updated in PopulateCommandList
}

void UpdateQuadBuffer() {
    if (g_quads[0].total == 0) return;

    Vertex quadVertices[100 * 6];
    int vertexIndex = 0;

    for (int i = 1; i < g_quads[0].total + 1; i++) {
        XMFLOAT4 color(g_quads[i].r, g_quads[i].g, g_quads[i].b, 1.0f);
        
        // Create two triangles per quad
        quadVertices[vertexIndex++] = { XMFLOAT3(static_cast<float>(g_quads[i].x1), static_cast<float>(g_quads[i].y1), static_cast<float>(g_quads[i].z1)), color };
        quadVertices[vertexIndex++] = { XMFLOAT3(static_cast<float>(g_quads[i].x2), static_cast<float>(g_quads[i].y2), static_cast<float>(g_quads[i].z2)), color };
        quadVertices[vertexIndex++] = { XMFLOAT3(static_cast<float>(g_quads[i].x3), static_cast<float>(g_quads[i].y3), static_cast<float>(g_quads[i].z3)), color };
        
        quadVertices[vertexIndex++] = { XMFLOAT3(static_cast<float>(g_quads[i].x1), static_cast<float>(g_quads[i].y1), static_cast<float>(g_quads[i].z1)), color };
        quadVertices[vertexIndex++] = { XMFLOAT3(static_cast<float>(g_quads[i].x3), static_cast<float>(g_quads[i].y3), static_cast<float>(g_quads[i].z3)), color };
        quadVertices[vertexIndex++] = { XMFLOAT3(static_cast<float>(g_quads[i].x4), static_cast<float>(g_quads[i].y4), static_cast<float>(g_quads[i].z4)), color };
    }

    UINT8* pVertexDataBegin;
    D3D12_RANGE readRange = { 0, 0 };
    g_vertexBufferQuads->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    memcpy(pVertexDataBegin, quadVertices, vertexIndex * sizeof(Vertex));
    g_vertexBufferQuads->Unmap(0, nullptr);
}

void AddQuad() {
    g_quads[0].state++;
    if (g_quads[0].state > 4) {
        g_quads[0].state = 1;
    }
    int st = g_quads[0].state;
    if (st == 1) {
        g_quads[0].total++;
        g_cn = g_quads[0].total;
    }
    if (st == 1) {
        g_quads[g_cn].x1 = g_cx;
        g_quads[g_cn].y1 = g_cy;
        g_quads[g_cn].z1 = g_cz;
    }
    if (st == 1 || st == 2) {
        g_quads[g_cn].x2 = g_cx;
        g_quads[g_cn].y2 = g_cy;
        g_quads[g_cn].z2 = g_cz;
    }
    if (st == 1 || st == 2 || st == 3) {
        g_quads[g_cn].x3 = g_cx;
        g_quads[g_cn].y3 = g_cy;
        g_quads[g_cn].z3 = g_cz;
    }
    if (st == 1 || st == 2 || st == 3 || st == 4) {
        g_quads[g_cn].x4 = g_cx;
        g_quads[g_cn].y4 = g_cy;
        g_quads[g_cn].z4 = g_cz;
    }
}

void HandleKeyboard() {
    // Movement
    if (g_keys['W'] || g_keys['w']) { g_cz -= 1; }
    if (g_keys['S'] || g_keys['s']) { g_cz += 1; }
    if (g_keys['A'] || g_keys['a']) { g_cx -= 1; }
    if (g_keys['D'] || g_keys['d']) { g_cx += 1; }
    if (g_keys['Q'] || g_keys['q']) { g_cy += 1; }
    if (g_keys['Z'] || g_keys['z']) { g_cy -= 1; }
    
    // Add quad
    if (g_keys[VK_SPACE]) {
        AddQuad();
        g_keys[VK_SPACE] = false; // Prevent multiple adds per keypress
    }
    
    // Color changes
    if (g_keys['R'] || g_keys['r']) {
        if (g_cn > 0) {
            g_quads[g_cn].r = 1.0f;
            g_quads[g_cn].g = 0.0f;
            g_quads[g_cn].b = 0.0f;
        }
        g_keys['R'] = false;
        g_keys['r'] = false;
    }
    if (g_keys['G'] || g_keys['g']) {
        if (g_cn > 0) {
            g_quads[g_cn].r = 0.0f;
            g_quads[g_cn].g = 1.0f;
            g_quads[g_cn].b = 0.0f;
        }
        g_keys['G'] = false;
        g_keys['g'] = false;
    }
    if (g_keys['B'] || g_keys['b']) {
        if (g_cn > 0) {
            g_quads[g_cn].r = 0.0f;
            g_quads[g_cn].g = 0.0f;
            g_quads[g_cn].b = 1.0f;
        }
        g_keys['B'] = false;
        g_keys['b'] = false;
    }
    if (g_keys['Y'] || g_keys['y']) {
        if (g_cn > 0) {
            g_quads[g_cn].r = 1.0f;
            g_quads[g_cn].g = 1.0f;
            g_quads[g_cn].b = 0.0f;
        }
        g_keys['Y'] = false;
        g_keys['y'] = false;
    }
}

void DrawGrid() {
    // Handled in PopulateCommandList
}

void DrawCube() {
    // Handled in PopulateCommandList
}

void DrawQuads() {
    // Handled in PopulateCommandList
}
