//https://www.shadertoy.com/view/clXXDl

#include <windows.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <chrono>
#include <cstdio>

#include "d3dx12.h"  // For CD3DX12 helper classes

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace D3DX12;  // Add this line to use the D3DX12 namespace

// PulsingGutsSelfExercise: A DirectX 12 project for self-exercise with pulsing guts effect

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

// Shader switching variables
ComPtr<ID3DBlob> g_vertexShader;
ComPtr<ID3DBlob> g_pixelShaderFinal;
ComPtr<ID3DBlob> g_pixelShader01;
ComPtr<ID3DBlob> g_pixelShader02;
int g_currentShader = 1; // 0 = pixel_shader_final.hlsl, 1 = pixel_shader_01.hlsl (default), 2 = pixel_shader_02.hlsl

// Add global for constant buffer
struct ShaderToyConstants {
    float iResolution[2];
    float iTime;
    float pad; // Padding for 16-byte alignment
};
ComPtr<ID3D12Resource> g_constantBuffer;
ShaderToyConstants g_cbData = {};
UINT8* g_pCbvDataBegin = nullptr;

// Vertex structure (no color needed)
struct Vertex {
    XMFLOAT3 position;
};

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitializeWindow(HINSTANCE hInstance);
void InitializeDirect3D();
void CreatePipelineState();
void CreateVertexBuffer();
void PopulateCommandList();
void WaitForGpu();
void MoveToNextFrame();
void RecreatePipelineState();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitializeWindow(hInstance);
    InitializeDirect3D();
    CreatePipelineState();
    CreateVertexBuffer();

    // High-precision timer setup
    static auto startTime = std::chrono::high_resolution_clock::now();

    // Main message loop
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Update iTime with high-precision timer
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = now - startTime;
            g_cbData.iTime = elapsed.count();
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
            else if (wParam == '0') {
                // Switch to original shader (pixel_shader_final.hlsl)
                if (g_currentShader != 0) {
                    g_currentShader = 0;
                    RecreatePipelineState();
                    OutputDebugStringA("Switched to pixel_shader_final.hlsl\n");
                }
                return 0;
            }
            else if (wParam == '1') {
                // Switch to new shader (pixel_shader_01.hlsl)
                if (g_currentShader != 1) {
                    g_currentShader = 1;
                    RecreatePipelineState();
                    OutputDebugStringA("Switched to pixel_shader_01.hlsl\n");
                }
                return 0;
            }
            else if (wParam == '2') {
                // Switch to new shader (pixel_shader_02.hlsl)
                if (g_currentShader != 2) {
                    g_currentShader = 2;
                    RecreatePipelineState();
                    OutputDebugStringA("Switched to pixel_shader_02.hlsl\n");
                }
                return 0;
            }
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void InitializeWindow(HINSTANCE hInstance) {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); // Set consistent arrow cursor
    wc.lpszClassName = L"PulsingGutsSelfExercise";
    RegisterClassEx(&wc);

    RECT windowRect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Calculate center position
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;
    int centerX = (screenWidth - windowWidth) / 2;
    int centerY = (screenHeight - windowHeight) / 2;

    g_hwnd = CreateWindow(
        L"PulsingGutsSelfExercise",
        L"PulsingGutsSelfExercise",
        WS_OVERLAPPEDWINDOW,
        centerX, centerY,
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
}

void CreatePipelineState() {
    // Create root signature with one constant buffer
    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(1, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        if (error) {
            OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        }
        return;
    }
    g_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&g_rootSignature));

    // Load all shaders
    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // Get the current directory and construct full paths
    wchar_t currentDir[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, currentDir);
    wchar_t vertexShaderPath[MAX_PATH];
    wchar_t pixelShaderFinalPath[MAX_PATH];
    wchar_t pixelShader01Path[MAX_PATH];
    wchar_t pixelShader02Path[MAX_PATH];
    swprintf_s(vertexShaderPath, L"%s\\vertex_shader.hlsl", currentDir);
    swprintf_s(pixelShaderFinalPath, L"%s\\pixel_shader_final.hlsl", currentDir);
    swprintf_s(pixelShader01Path, L"%s\\pixel_shader_01.hlsl", currentDir);
    swprintf_s(pixelShader02Path, L"%s\\pixel_shader_02.hlsl", currentDir);
    
    // Load vertex shader
    OutputDebugStringA("Loading vertex shader...\n");
    if (FAILED(D3DCompileFromFile(vertexShaderPath, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &g_vertexShader, &error))) {
        if (error) {
            OutputDebugStringA("Vertex shader compilation failed:\n");
            OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        } else {
            OutputDebugStringA("Vertex shader compilation failed with no error details\n");
        }
        return;
    }
    OutputDebugStringA("Vertex shader loaded successfully\n");
    
    // Load pixel shader final
    OutputDebugStringA("Loading pixel_shader_final.hlsl...\n");
    if (FAILED(D3DCompileFromFile(pixelShaderFinalPath, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_pixelShaderFinal, &error))) {
        if (error) {
            OutputDebugStringA("Pixel shader final compilation failed:\n");
            OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        } else {
            OutputDebugStringA("Pixel shader final compilation failed with no error details\n");
        }
        return;
    }
    OutputDebugStringA("Pixel shader final loaded successfully\n");
    
    // Load pixel shader 01
    OutputDebugStringA("Loading pixel_shader_01.hlsl...\n");
    if (FAILED(D3DCompileFromFile(pixelShader01Path, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_pixelShader01, &error))) {
        if (error) {
            OutputDebugStringA("Pixel shader 01 compilation failed:\n");
            OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        } else {
            OutputDebugStringA("Pixel shader 01 compilation failed with no error details\n");
        }
        return;
    }
    OutputDebugStringA("Pixel shader 01 loaded successfully\n");
    
    // Load pixel shader 02
    OutputDebugStringA("Loading pixel_shader_02.hlsl...\n");
    if (FAILED(D3DCompileFromFile(pixelShader02Path, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &g_pixelShader02, &error))) {
        if (error) {
            OutputDebugStringA("Pixel shader 02 compilation failed:\n");
            OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        } else {
            OutputDebugStringA("Pixel shader 02 compilation failed with no error details\n");
        }
        return;
    }
    OutputDebugStringA("Pixel shader 02 loaded successfully\n");

    // Create initial pipeline state with the default shader (pixel_shader_01.hlsl)
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = g_rootSignature.Get();
    psoDesc.VS = { g_vertexShader->GetBufferPointer(), g_vertexShader->GetBufferSize() };
    psoDesc.PS = { g_pixelShader01->GetBufferPointer(), g_pixelShader01->GetBufferSize() };
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    if (FAILED(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelineState)))) {
        return;
    }

    // Create constant buffer
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC cbDesc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);
    g_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_constantBuffer)
    );
    CD3DX12_RANGE readRange(0, 0);
    g_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&g_pCbvDataBegin));
}

void CreateVertexBuffer() {
    // Full-screen triangle vertices (NDC)
    Vertex triangleVertices[] = {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f) }, // Bottom left
        { XMFLOAT3(3.0f, -1.0f, 0.0f) },  // Bottom right (off-screen)
        { XMFLOAT3(-1.0f, 3.0f, 0.0f) }   // Top left (off-screen)
    };
    const UINT vertexBufferSize = sizeof(triangleVertices);
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
    memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
    g_vertexBuffer->Unmap(0, nullptr);
}

void PopulateCommandList() {
    g_commandAllocator->Reset();
    g_commandList->Reset(g_commandAllocator.Get(), g_pipelineState.Get());
    g_commandList->SetGraphicsRootSignature(g_rootSignature.Get());
    // Update constant buffer
    g_cbData.iResolution[0] = static_cast<float>(WINDOW_WIDTH);
    g_cbData.iResolution[1] = static_cast<float>(WINDOW_HEIGHT);
    // g_cbData.iTime is set in WinMain using high-precision timer
    memcpy(g_pCbvDataBegin, &g_cbData, sizeof(g_cbData));
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
        sizeof(Vertex) * 3,                      // SizeInBytes
        sizeof(Vertex)                           // StrideInBytes
    };
    g_commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    g_commandList->DrawInstanced(3, 1, 0, 0);
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(g_renderTargets[g_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
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

void RecreatePipelineState() {
    // Wait for GPU to finish before recreating pipeline state
    WaitForGpu();
    
    // Release the current pipeline state
    g_pipelineState.Reset();
    
    // Create new pipeline state with the selected shader
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = g_rootSignature.Get();
    psoDesc.VS = { g_vertexShader->GetBufferPointer(), g_vertexShader->GetBufferSize() };
    
    // Select the appropriate pixel shader based on current selection
    if (g_currentShader == 0) {
        psoDesc.PS = { g_pixelShaderFinal->GetBufferPointer(), g_pixelShaderFinal->GetBufferSize() };
    } else if (g_currentShader == 1) {
        psoDesc.PS = { g_pixelShader01->GetBufferPointer(), g_pixelShader01->GetBufferSize() };
    } else {
        psoDesc.PS = { g_pixelShader02->GetBufferPointer(), g_pixelShader02->GetBufferSize() };
    }
    
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    
    if (FAILED(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelineState)))) {
        OutputDebugStringA("Failed to recreate pipeline state\n");
        return;
    }
    
    OutputDebugStringA("Pipeline state recreated successfully\n");
} 