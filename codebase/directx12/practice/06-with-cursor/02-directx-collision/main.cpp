#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXCollision.h>
#include <wrl/client.h>

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

// Game boundaries
const float BOUNDARY_SIZE = 5.0f;
const float CUBE_SIZE = 0.5f;
const float CUBE_SPEED = 2.0f;
const float CAMERA_DISTANCE = 10.0f;

// Global variables
HWND g_hwnd = nullptr;
ComPtr<ID3D12Device> g_device;
ComPtr<ID3D12CommandQueue> g_commandQueue;
ComPtr<ID3D12CommandAllocator> g_commandAllocator;
ComPtr<ID3D12GraphicsCommandList> g_commandList;
ComPtr<ID3D12RootSignature> g_rootSignature;
ComPtr<ID3D12PipelineState> g_pipelineState;
ComPtr<ID3D12Resource> g_vertexBuffer;
ComPtr<ID3D12DescriptorHeap> g_rtvHeap;
ComPtr<ID3D12Resource> g_renderTargets[2];
UINT g_rtvDescriptorSize;
UINT g_frameIndex;
ComPtr<ID3D12Fence> g_fence;
UINT64 g_fenceValue;
HANDLE g_fenceEvent;
ComPtr<IDXGISwapChain3> g_swapChain;
ComPtr<ID3D12Resource> g_constantBuffer;
UINT8* g_pCbvDataBegin = nullptr;

// Game state
struct GameState {
    XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT3 velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
    BoundingBox cubeBounds;
    BoundingBox worldBounds;
    float rotationY = 0.0f;
    float rotationZ = 0.0f;
    float rotationSpeed = 1.0f;
} g_gameState;

// Vertex structure
struct Vertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
};

// Add after other global variables
struct ConstantBuffer {
    XMFLOAT4X4 World;
    XMFLOAT4X4 View;
    XMFLOAT4X4 Projection;
};

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitializeWindow(HINSTANCE hInstance);
void InitializeDirect3D();
void CreatePipelineState();
void CreateVertexBuffer();
void CreateConstantBuffer();
void UpdateGameState(float deltaTime);
void PopulateCommandList();
void WaitForGpu();
void MoveToNextFrame();

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    InitializeWindow(hInstance);
    InitializeDirect3D();
    CreatePipelineState();
    CreateVertexBuffer();
    CreateConstantBuffer();

    // Initialize game state
    g_gameState.position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    g_gameState.velocity = XMFLOAT3(CUBE_SPEED, CUBE_SPEED, 0.0f);
    g_gameState.cubeBounds = BoundingBox(g_gameState.position, XMFLOAT3(CUBE_SIZE, CUBE_SIZE, CUBE_SIZE));
    g_gameState.worldBounds = BoundingBox(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(BOUNDARY_SIZE, BOUNDARY_SIZE, BOUNDARY_SIZE));
    g_gameState.rotationY = 0.0f;
    g_gameState.rotationZ = 0.0f;
    g_gameState.rotationSpeed = 1.0f;

    // Main message loop
    MSG msg = {};
    LARGE_INTEGER frequency;
    LARGE_INTEGER lastTime;
    LARGE_INTEGER currentTime;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Calculate delta time
            QueryPerformanceCounter(&currentTime);
            float deltaTime = static_cast<float>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            lastTime = currentTime;

            // Update game state
            UpdateGameState(deltaTime);

            // Render frame
            PopulateCommandList();
            WaitForGpu();
            g_swapChain->Present(1, 0);
            MoveToNextFrame();
        }
    }

    return static_cast<int>(msg.wParam);
}

void UpdateGameState(float deltaTime) {
    // Update position
    g_gameState.position.x += g_gameState.velocity.x * deltaTime;
    g_gameState.position.y += g_gameState.velocity.y * deltaTime;
    g_gameState.position.z += g_gameState.velocity.z * deltaTime;

    // Update cube bounds
    g_gameState.cubeBounds.Center = g_gameState.position;

    // Calculate aspect ratio
    float aspectRatio = static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT;
    float boundaryWidth = BOUNDARY_SIZE * aspectRatio;

    // Check collisions with world boundaries
    float minX = -boundaryWidth / 2.0f;
    float maxX = boundaryWidth / 2.0f;
    float minY = -BOUNDARY_SIZE / 2.0f;
    float maxY = BOUNDARY_SIZE / 2.0f;
    float minZ = -BOUNDARY_SIZE / 2.0f;
    float maxZ = BOUNDARY_SIZE / 2.0f;

    if (g_gameState.position.x - CUBE_SIZE < minX || g_gameState.position.x + CUBE_SIZE > maxX) {
        g_gameState.velocity.x = -g_gameState.velocity.x;
    }
    if (g_gameState.position.y - CUBE_SIZE < minY || g_gameState.position.y + CUBE_SIZE > maxY) {
        g_gameState.velocity.y = -g_gameState.velocity.y;
    }
    if (g_gameState.position.z - CUBE_SIZE < minZ || g_gameState.position.z + CUBE_SIZE > maxZ) {
        g_gameState.velocity.z = -g_gameState.velocity.z;
    }

    // Update rotation
    g_gameState.rotationY += g_gameState.rotationSpeed * deltaTime;
    g_gameState.rotationZ += g_gameState.rotationSpeed * 0.5f * deltaTime;  // Slightly slower Z rotation
}

void InitializeWindow(HINSTANCE hInstance) {
    // Register window class
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"D3D12CollisionDemo";
    RegisterClassEx(&wcex);

    // Create window
    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindow(
        L"D3D12CollisionDemo",
        L"DirectX 12 Collision Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
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

    // Create DXGI factory
    ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)))) {
        OutputDebugStringA("Failed to create DXGI factory\n");
        return;
    }

    // Create device
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)))) {
        OutputDebugStringA("Failed to create D3D12 device\n");
        return;
    }

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue)))) {
        OutputDebugStringA("Failed to create command queue\n");
        return;
    }

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = WINDOW_WIDTH;
    swapChainDesc.Height = WINDOW_HEIGHT;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(factory->CreateSwapChainForHwnd(
        g_commandQueue.Get(),
        g_hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1))) {
        OutputDebugStringA("Failed to create swap chain\n");
        return;
    }

    if (FAILED(swapChain1.As(&g_swapChain))) {
        OutputDebugStringA("Failed to get swap chain 3\n");
        return;
    }

    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heap for render target views
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 2;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap)))) {
        OutputDebugStringA("Failed to create RTV heap\n");
        return;
    }

    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create render target views
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < 2; i++) {
        if (FAILED(g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i])))) {
            OutputDebugStringA("Failed to get swap chain buffer\n");
            return;
        }
        g_device->CreateRenderTargetView(g_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, g_rtvDescriptorSize);
    }

    // Create command allocator
    if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator)))) {
        OutputDebugStringA("Failed to create command allocator\n");
        return;
    }

    // Create command list
    if (FAILED(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_commandList)))) {
        OutputDebugStringA("Failed to create command list\n");
        return;
    }

    // Create fence
    if (FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) {
        OutputDebugStringA("Failed to create fence\n");
        return;
    }
    g_fenceValue = 1;

    // Create fence event
    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (g_fenceEvent == nullptr) {
        OutputDebugStringA("Failed to create fence event\n");
        return;
    }
}

void WaitForGpu() {
    // Schedule a signal command in the queue
    const UINT64 fence = g_fenceValue;
    if (FAILED(g_commandQueue->Signal(g_fence.Get(), fence))) {
        OutputDebugStringA("Failed to signal fence\n");
        return;
    }
    g_fenceValue++;

    // Wait until the fence has been processed
    if (g_fence->GetCompletedValue() < fence) {
        if (FAILED(g_fence->SetEventOnCompletion(fence, g_fenceEvent))) {
            OutputDebugStringA("Failed to set fence event\n");
            return;
        }
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

void MoveToNextFrame() {
    // Schedule a signal command in the queue
    const UINT64 currentFenceValue = g_fenceValue;
    if (FAILED(g_commandQueue->Signal(g_fence.Get(), currentFenceValue))) {
        OutputDebugStringA("Failed to signal fence\n");
        return;
    }
    g_fenceValue++;

    // Update the frame index
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready
    if (g_fence->GetCompletedValue() < currentFenceValue) {
        if (FAILED(g_fence->SetEventOnCompletion(currentFenceValue, g_fenceEvent))) {
            OutputDebugStringA("Failed to set fence event\n");
            return;
        }
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

void CreatePipelineState() {
    // Create root signature
    D3D12_ROOT_PARAMETER rootParameters[1];
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        OutputDebugStringA("Failed to serialize root signature\n");
        return;
    }
    if (FAILED(g_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&g_rootSignature)))) {
        OutputDebugStringA("Failed to create root signature\n");
        return;
    }

    // Create the pipeline state
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Define the vertex shader
    const char* vertexShader = R"(
        struct PSInput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };

        cbuffer Constants : register(b0) {
            float4x4 World;
            float4x4 View;
            float4x4 Projection;
        }

        PSInput VSMain(float3 position : POSITION, float4 color : COLOR) {
            PSInput result;
            float4 worldPos = mul(World, float4(position, 1.0f));
            float4 viewPos = mul(View, worldPos);
            result.position = mul(Projection, viewPos);
            result.color = color;
            return result;
        }
    )";

    // Define the pixel shader
    const char* pixelShader = R"(
        struct PSInput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };

        float4 PSMain(PSInput input) : SV_TARGET {
            return input.color;
        }
    )";

    // Compile shaders
    ComPtr<ID3DBlob> vertexShaderBlob;
    ComPtr<ID3DBlob> pixelShaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    if (FAILED(D3DCompile(vertexShader, strlen(vertexShader), "VSMain", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShaderBlob, &errorBlob))) {
        OutputDebugStringA("Failed to compile vertex shader\n");
        return;
    }

    if (FAILED(D3DCompile(pixelShader, strlen(pixelShader), "PSMain", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShaderBlob, &errorBlob))) {
        OutputDebugStringA("Failed to compile pixel shader\n");
        return;
    }

    // Create pipeline state object
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = g_rootSignature.Get();
    psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    if (FAILED(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelineState)))) {
        OutputDebugStringA("Failed to create pipeline state\n");
        return;
    }
}

void CreateVertexBuffer() {
    // Create vertex buffer for a cube with proper winding order
    // Each face is made of 2 triangles (6 vertices)
    Vertex cubeVertices[] = {
        // Front face (2 triangles)
        { XMFLOAT3(-CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },

        // Back face (2 triangles)
        { XMFLOAT3( CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

        // Top face (2 triangles)
        { XMFLOAT3(-CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },

        // Bottom face (2 triangles)
        { XMFLOAT3(-CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },

        // Right face (2 triangles)
        { XMFLOAT3( CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3( CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },

        // Left face (2 triangles)
        { XMFLOAT3(-CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) }
    };

    const UINT vertexBufferSize = sizeof(cubeVertices);
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
    g_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_vertexBuffer)
    );

    UINT8* pVertexDataBegin;
    D3D12_RANGE readRange = { 0, 0 };
    g_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    memcpy(pVertexDataBegin, cubeVertices, sizeof(cubeVertices));
    g_vertexBuffer->Unmap(0, nullptr);
}

void CreateConstantBuffer() {
    const UINT constantBufferSize = sizeof(ConstantBuffer);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
    if (FAILED(g_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_constantBuffer)))) {
        OutputDebugStringA("Failed to create constant buffer\n");
        return;
    }

    // Map and initialize the constant buffer
    D3D12_RANGE readRange = { 0, 0 };
    if (FAILED(g_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&g_pCbvDataBegin)))) {
        OutputDebugStringA("Failed to map constant buffer\n");
        return;
    }

    // Calculate aspect ratio
    float aspectRatio = static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT;
    float boundaryWidth = BOUNDARY_SIZE * aspectRatio;

    // Initialize with identity matrices
    ConstantBuffer constantBuffer = {};
    XMStoreFloat4x4(&constantBuffer.World, XMMatrixIdentity());
    XMStoreFloat4x4(&constantBuffer.View, XMMatrixLookAtLH(
        XMVectorSet(0.0f, 0.0f, -CAMERA_DISTANCE, 0.0f),  // Eye position
        XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),             // Focus position
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)              // Up direction
    ));
    XMStoreFloat4x4(&constantBuffer.Projection, XMMatrixOrthographicLH(
        boundaryWidth,    // Width
        BOUNDARY_SIZE,    // Height
        0.1f,            // Near plane
        100.0f           // Far plane
    ));
    memcpy(g_pCbvDataBegin, &constantBuffer, sizeof(constantBuffer));
}

void PopulateCommandList() {
    g_commandAllocator->Reset();
    g_commandList->Reset(g_commandAllocator.Get(), g_pipelineState.Get());

    g_commandList->SetGraphicsRootSignature(g_rootSignature.Get());
    
    // Update world matrix with both Y and Z rotations
    XMMATRIX worldMatrix = XMMatrixRotationZ(g_gameState.rotationZ) * 
                          XMMatrixRotationY(g_gameState.rotationY) * 
                          XMMatrixTranslation(g_gameState.position.x, g_gameState.position.y, g_gameState.position.z);
    XMFLOAT4X4 worldMatrixFloat;
    XMStoreFloat4x4(&worldMatrixFloat, worldMatrix);

    // Calculate aspect ratio
    float aspectRatio = static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT;
    float boundaryWidth = BOUNDARY_SIZE * aspectRatio;

    // Update constant buffer
    ConstantBuffer constantBuffer;
    constantBuffer.World = worldMatrixFloat;
    XMStoreFloat4x4(&constantBuffer.View, XMMatrixLookAtLH(
        XMVectorSet(0.0f, 0.0f, -CAMERA_DISTANCE, 0.0f),  // Eye position
        XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),             // Focus position
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)              // Up direction
    ));
    XMStoreFloat4x4(&constantBuffer.Projection, XMMatrixOrthographicLH(
        boundaryWidth,    // Width
        BOUNDARY_SIZE,    // Height
        0.1f,            // Near plane
        100.0f           // Far plane
    ));
    memcpy(g_pCbvDataBegin, &constantBuffer, sizeof(constantBuffer));

    g_commandList->SetGraphicsRootConstantBufferView(0, g_constantBuffer->GetGPUVirtualAddress());

    D3D12_VIEWPORT viewport{
        0.0f,                                    // TopLeftX
        0.0f,                                    // TopLeftY
        static_cast<float>(WINDOW_WIDTH),        // Width
        static_cast<float>(WINDOW_HEIGHT),       // Height
        0.0f,                                    // MinDepth
        1.0f                                     // MaxDepth
    };
    D3D12_RECT scissorRect{
        0,              // left
        0,              // top
        WINDOW_WIDTH,   // right
        WINDOW_HEIGHT   // bottom
    };
    g_commandList->RSSetViewports(1, &viewport);
    g_commandList->RSSetScissorRects(1, &scissorRect);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(g_renderTargets[g_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    g_commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += g_frameIndex * g_rtvDescriptorSize;
    g_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    g_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{
        g_vertexBuffer->GetGPUVirtualAddress(),  // BufferLocation
        sizeof(Vertex) * 36,                     // SizeInBytes (6 faces * 6 vertices)
        sizeof(Vertex)                           // StrideInBytes
    };
    g_commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

    // Draw each face of the cube (6 vertices per face)
    for (int i = 0; i < 6; i++) {
        g_commandList->DrawInstanced(6, 1, i * 6, 0);
    }

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(g_renderTargets[g_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    g_commandList->ResourceBarrier(1, &barrier);

    g_commandList->Close();

    ID3D12CommandList* ppCommandLists[] = { g_commandList.Get() };
    g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
        }
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
} 