#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <d3dcompiler.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Vec4 {
    float x;
    float y;
    float z;
    float w;
};

struct Mat4 {
    float m[4][4];
};

struct Vertex {
    float clipX;
    float clipY;
    float clipZ;
    float clipW;
    float r;
    float g;
    float b;
    float a;
};

static Mat4 Identity() {
    Mat4 out = {};
    out.m[0][0] = 1.0f;
    out.m[1][1] = 1.0f;
    out.m[2][2] = 1.0f;
    out.m[3][3] = 1.0f;
    return out;
}

static Mat4 Multiply(const Mat4& a, const Mat4& b) {
    Mat4 out = {};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            out.m[row][col] =
                a.m[row][0] * b.m[0][col] +
                a.m[row][1] * b.m[1][col] +
                a.m[row][2] * b.m[2][col] +
                a.m[row][3] * b.m[3][col];
        }
    }
    return out;
}

static Vec4 TransformPoint(const Mat4& m, const Vec3& v) {
    Vec4 out = {};
    out.x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3];
    out.y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3];
    out.z = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3];
    out.w = m.m[3][0] * v.x + m.m[3][1] * v.y + m.m[3][2] * v.z + m.m[3][3];
    return out;
}

static Vec3 Subtract(const Vec3& a, const Vec3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

static float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static float Length(const Vec3& v) {
    return std::sqrt(Dot(v, v));
}

static Vec3 Normalize(const Vec3& v) {
    const float len = Length(v);
    if (len <= 0.000001f) {
        return { 0.0f, 0.0f, 0.0f };
    }
    const float inv = 1.0f / len;
    return { v.x * inv, v.y * inv, v.z * inv };
}

static Mat4 LookAtLH(const Vec3& eye, const Vec3& target, const Vec3& up) {
    const Vec3 zaxis = Normalize(Subtract(target, eye));
    const Vec3 xaxis = Normalize(Cross(up, zaxis));
    const Vec3 yaxis = Cross(zaxis, xaxis);

    Mat4 out = Identity();
    out.m[0][0] = xaxis.x;
    out.m[0][1] = xaxis.y;
    out.m[0][2] = xaxis.z;
    out.m[0][3] = -Dot(xaxis, eye);

    out.m[1][0] = yaxis.x;
    out.m[1][1] = yaxis.y;
    out.m[1][2] = yaxis.z;
    out.m[1][3] = -Dot(yaxis, eye);

    out.m[2][0] = zaxis.x;
    out.m[2][1] = zaxis.y;
    out.m[2][2] = zaxis.z;
    out.m[2][3] = -Dot(zaxis, eye);

    return out;
}

static Mat4 PerspectiveFovLH(float fovYRadians, float aspect, float zNear, float zFar) {
    const float yScale = 1.0f / std::tan(fovYRadians * 0.5f);
    const float xScale = yScale / aspect;

    Mat4 out = {};
    out.m[0][0] = xScale;
    out.m[1][1] = yScale;
    out.m[2][2] = zFar / (zFar - zNear);
    out.m[2][3] = (-zNear * zFar) / (zFar - zNear);
    out.m[3][2] = 1.0f;
    return out;
}

static std::vector<std::uint8_t> CompileShader(const char* source, const char* entryPoint, const char* target) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
    flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    const HRESULT hr = D3DCompile(
        source,
        std::strlen(source),
        nullptr,
        nullptr,
        nullptr,
        entryPoint,
        target,
        flags,
        0,
        &shaderBlob,
        &errorBlob);

    if (FAILED(hr)) {
        std::string message = "D3DCompile failed";
        if (errorBlob != nullptr) {
            message += ": ";
            message.append(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
            errorBlob->Release();
        }
        if (shaderBlob != nullptr) {
            shaderBlob->Release();
        }
        throw std::runtime_error(message);
    }

    if (errorBlob != nullptr) {
        errorBlob->Release();
    }

    std::vector<std::uint8_t> bytes(shaderBlob->GetBufferSize());
    std::memcpy(bytes.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
    shaderBlob->Release();
    return bytes;
}

static const char* kVertexShaderSource = R"(
struct VSIn {
    float4 clipPos : TEXCOORD0;
    float4 color   : TEXCOORD1;
};

struct VSOut {
    float4 position : SV_Position;
    float4 color    : TEXCOORD0;
};

VSOut VSMain(VSIn input) {
    VSOut output;
    output.position = input.clipPos;
    output.color = input.color;
    return output;
}
)";

static const char* kPixelShaderSource = R"(
struct PSIn {
    float4 position : SV_Position;
    float4 color    : TEXCOORD0;
};

float4 PSMain(PSIn input) : SV_Target0 {
    return input.color;
}
)";

class App {
public:
    bool Initialize();
    void Run();
    void Shutdown();

private:
    struct Face {
        int i0;
        int i1;
        int i2;
        float r;
        float g;
        float b;
        float a;
    };

    void HandleEvent(const SDL_Event& e);
    void RecreatePipelineIfNeeded();
    void UpdateVertices(std::vector<Vertex>& outVertices, int drawableWidth, int drawableHeight);
    void RenderFrame();
    bool CreatePipeline();
    void DestroyPipeline();
    bool CreateBuffers();
    void DestroyBuffers();
    void LogSdlError(const char* context);

    SDL_Window* m_window = nullptr;
    SDL_GPUDevice* m_device = nullptr;
    SDL_GPUGraphicsPipeline* m_pipeline = nullptr;
    SDL_GPUBuffer* m_vertexBuffer = nullptr;
    SDL_GPUTransferBuffer* m_uploadBuffer = nullptr;

    SDL_GPUTextureFormat m_pipelineSwapchainFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

    bool m_running = true;
    bool m_orbiting = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;

    float m_yaw = 0.75f;
    float m_pitch = 0.45f;
    float m_radius = 5.0f;
};

bool App::Initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LogSdlError("SDL_Init");
        return false;
    }

    m_window = SDL_CreateWindow(
        "SDL3 GPU - Orbiting Tetrahedron",
        1280,
        720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (m_window == nullptr) {
        LogSdlError("SDL_CreateWindow");
        return false;
    }

    m_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXBC, true, "direct3d12");
    if (m_device == nullptr) {
        LogSdlError("SDL_CreateGPUDevice");
        return false;
    }

    if (!SDL_ClaimWindowForGPUDevice(m_device, m_window)) {
        LogSdlError("SDL_ClaimWindowForGPUDevice");
        return false;
    }

    if (!CreateBuffers()) {
        return false;
    }

    if (!CreatePipeline()) {
        return false;
    }

    return true;
}

void App::Run() {
    while (m_running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            HandleEvent(e);
        }
        RenderFrame();
    }
}

void App::Shutdown() {
    DestroyPipeline();
    DestroyBuffers();

    if (m_device != nullptr && m_window != nullptr) {
        SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
    }

    if (m_device != nullptr) {
        SDL_DestroyGPUDevice(m_device);
        m_device = nullptr;
    }

    if (m_window != nullptr) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void App::HandleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_QUIT:
        m_running = false;
        break;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        m_running = false;
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button == SDL_BUTTON_MIDDLE) {
            m_orbiting = true;
            m_lastMouseX = e.button.x;
            m_lastMouseY = e.button.y;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_MIDDLE) {
            m_orbiting = false;
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (m_orbiting) {
            const int dx = e.motion.x - m_lastMouseX;
            const int dy = e.motion.y - m_lastMouseY;
            m_lastMouseX = e.motion.x;
            m_lastMouseY = e.motion.y;

            m_yaw += dx * 0.01f;
            m_pitch += dy * 0.01f;
            m_pitch = std::clamp(m_pitch, -1.4f, 1.4f);
        }
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        m_radius -= e.wheel.y * 0.35f;
        m_radius = std::clamp(m_radius, 2.0f, 12.0f);
        break;

    default:
        break;
    }
}

bool App::CreateBuffers() {
    SDL_GPUBufferCreateInfo bufferInfo = {};
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferInfo.size = sizeof(Vertex) * 12;
    bufferInfo.props = 0;

    m_vertexBuffer = SDL_CreateGPUBuffer(m_device, &bufferInfo);
    if (m_vertexBuffer == nullptr) {
        LogSdlError("SDL_CreateGPUBuffer");
        return false;
    }

    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = sizeof(Vertex) * 12;
    transferInfo.props = 0;

    m_uploadBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferInfo);
    if (m_uploadBuffer == nullptr) {
        LogSdlError("SDL_CreateGPUTransferBuffer");
        return false;
    }

    return true;
}

void App::DestroyBuffers() {
    if (m_vertexBuffer != nullptr) {
        SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
        m_vertexBuffer = nullptr;
    }

    if (m_uploadBuffer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(m_device, m_uploadBuffer);
        m_uploadBuffer = nullptr;
    }
}

bool App::CreatePipeline() {
    try {
        const std::vector<std::uint8_t> vsBytecode = CompileShader(kVertexShaderSource, "VSMain", "vs_5_1");
        const std::vector<std::uint8_t> psBytecode = CompileShader(kPixelShaderSource, "PSMain", "ps_5_1");

        SDL_GPUShaderCreateInfo vsInfo = {};
        vsInfo.code_size = vsBytecode.size();
        vsInfo.code = vsBytecode.data();
        vsInfo.entrypoint = "VSMain";
        vsInfo.format = SDL_GPU_SHADERFORMAT_DXBC;
        vsInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vsInfo.num_samplers = 0;
        vsInfo.num_storage_textures = 0;
        vsInfo.num_storage_buffers = 0;
        vsInfo.num_uniform_buffers = 0;
        vsInfo.props = 0;

        SDL_GPUShaderCreateInfo psInfo = {};
        psInfo.code_size = psBytecode.size();
        psInfo.code = psBytecode.data();
        psInfo.entrypoint = "PSMain";
        psInfo.format = SDL_GPU_SHADERFORMAT_DXBC;
        psInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        psInfo.num_samplers = 0;
        psInfo.num_storage_textures = 0;
        psInfo.num_storage_buffers = 0;
        psInfo.num_uniform_buffers = 0;
        psInfo.props = 0;

        SDL_GPUShader* vertexShader = SDL_CreateGPUShader(m_device, &vsInfo);
        if (vertexShader == nullptr) {
            LogSdlError("SDL_CreateGPUShader(vertex)");
            return false;
        }

        SDL_GPUShader* fragmentShader = SDL_CreateGPUShader(m_device, &psInfo);
        if (fragmentShader == nullptr) {
            SDL_ReleaseGPUShader(m_device, vertexShader);
            LogSdlError("SDL_CreateGPUShader(fragment)");
            return false;
        }

        SDL_GPUVertexBufferDescription vertexBufferDesc = {};
        vertexBufferDesc.slot = 0;
        vertexBufferDesc.pitch = sizeof(Vertex);
        vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDesc.instance_step_rate = 0;

        std::array<SDL_GPUVertexAttribute, 2> attrs = {};
        attrs[0].location = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[0].offset = 0;

        attrs[1].location = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[1].offset = 16;

        SDL_GPUColorTargetDescription colorTarget = {};
        colorTarget.format = SDL_GetGPUSwapchainTextureFormat(m_device, m_window);
        colorTarget.blend_state.enable_blend = false;
        colorTarget.blend_state.enable_color_write_mask = false;

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertexShader;
        pipelineInfo.fragment_shader = fragmentShader;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBufferDesc;
        pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
        pipelineInfo.vertex_input_state.vertex_attributes = attrs.data();
        pipelineInfo.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attrs.size());
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pipelineInfo.rasterizer_state.enable_depth_bias = false;
        pipelineInfo.rasterizer_state.enable_depth_clip = true;
        pipelineInfo.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        pipelineInfo.multisample_state.sample_mask = 0;
        pipelineInfo.multisample_state.enable_mask = false;
        pipelineInfo.multisample_state.enable_alpha_to_coverage = false;
        pipelineInfo.depth_stencil_state.enable_depth_test = false;
        pipelineInfo.depth_stencil_state.enable_depth_write = false;
        pipelineInfo.depth_stencil_state.enable_stencil_test = false;
        pipelineInfo.target_info.color_target_descriptions = &colorTarget;
        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID;
        pipelineInfo.target_info.has_depth_stencil_target = false;
        pipelineInfo.props = 0;

        m_pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &pipelineInfo);
        m_pipelineSwapchainFormat = colorTarget.format;

        SDL_ReleaseGPUShader(m_device, fragmentShader);
        SDL_ReleaseGPUShader(m_device, vertexShader);

        if (m_pipeline == nullptr) {
            LogSdlError("SDL_CreateGPUGraphicsPipeline");
            return false;
        }

        return true;
    }
    catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return false;
    }
}

void App::DestroyPipeline() {
    if (m_pipeline != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
        m_pipeline = nullptr;
    }
    m_pipelineSwapchainFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
}

void App::RecreatePipelineIfNeeded() {
    const SDL_GPUTextureFormat currentFormat = SDL_GetGPUSwapchainTextureFormat(m_device, m_window);
    if (m_pipeline == nullptr || currentFormat != m_pipelineSwapchainFormat) {
        DestroyPipeline();
        CreatePipeline();
    }
}

void App::UpdateVertices(std::vector<Vertex>& outVertices, int drawableWidth, int drawableHeight) {
    outVertices.clear();
    outVertices.reserve(12);

    const std::array<Vec3, 4> points = {
        Vec3{  1.0f,  1.0f,  1.0f },
        Vec3{ -1.0f, -1.0f,  1.0f },
        Vec3{ -1.0f,  1.0f, -1.0f },
        Vec3{  1.0f, -1.0f, -1.0f }
    };

    std::array<Face, 4> faces = {
        Face{ 0, 1, 2, 0.90f, 0.20f, 0.20f, 1.0f },
        Face{ 0, 3, 1, 0.20f, 0.85f, 0.30f, 1.0f },
        Face{ 0, 2, 3, 0.20f, 0.45f, 0.95f, 1.0f },
        Face{ 1, 3, 2, 0.95f, 0.85f, 0.20f, 1.0f }
    };

    const Vec3 target = { 0.0f, 0.0f, 0.0f };
    const Vec3 up = { 0.0f, 1.0f, 0.0f };

    const float cp = std::cos(m_pitch);
    const float sp = std::sin(m_pitch);
    const float cy = std::cos(m_yaw);
    const float sy = std::sin(m_yaw);

    const Vec3 eye = {
        m_radius * cp * cy,
        m_radius * sp,
        m_radius * cp * sy
    };

    const float aspect = static_cast<float>(std::max(drawableWidth, 1)) / static_cast<float>(std::max(drawableHeight, 1));
    const Mat4 view = LookAtLH(eye, target, up);
    const Mat4 proj = PerspectiveFovLH(60.0f * 3.1415926535f / 180.0f, aspect, 0.1f, 100.0f);
    const Mat4 world = Identity();
    const Mat4 viewWorld = Multiply(view, world);
    const Mat4 clipFromWorld = Multiply(proj, viewWorld);

    struct FaceOrder {
        int index;
        float avgViewZ;
    };

    std::array<FaceOrder, 4> order = {};
    for (int i = 0; i < 4; ++i) {
        const Vec4 v0 = TransformPoint(viewWorld, points[faces[i].i0]);
        const Vec4 v1 = TransformPoint(viewWorld, points[faces[i].i1]);
        const Vec4 v2 = TransformPoint(viewWorld, points[faces[i].i2]);
        order[i].index = i;
        order[i].avgViewZ = (v0.z + v1.z + v2.z) / 3.0f;
    }

    std::sort(order.begin(), order.end(), [](const FaceOrder& a, const FaceOrder& b) {
        return a.avgViewZ > b.avgViewZ;
    });

    for (const FaceOrder& entry : order) {
        const Face& face = faces[entry.index];
        const int indices[3] = { face.i0, face.i1, face.i2 };
        for (int corner = 0; corner < 3; ++corner) {
            const Vec4 clip = TransformPoint(clipFromWorld, points[indices[corner]]);
            outVertices.push_back(Vertex{
                clip.x,
                clip.y,
                clip.z,
                clip.w,
                face.r,
                face.g,
                face.b,
                face.a
            });
        }
    }
}

void App::RenderFrame() {
    RecreatePipelineIfNeeded();
    if (m_pipeline == nullptr) {
        return;
    }

    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(m_device);
    if (commandBuffer == nullptr) {
        LogSdlError("SDL_AcquireGPUCommandBuffer");
        return;
    }

    SDL_GPUTexture* swapchainTexture = nullptr;
    Uint32 swapchainWidth = 0;
    Uint32 swapchainHeight = 0;

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, m_window, &swapchainTexture, &swapchainWidth, &swapchainHeight)) {
        LogSdlError("SDL_WaitAndAcquireGPUSwapchainTexture");
        SDL_CancelGPUCommandBuffer(commandBuffer);
        return;
    }

    if (swapchainTexture == nullptr || swapchainWidth == 0 || swapchainHeight == 0) {
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        return;
    }

    std::vector<Vertex> vertices;
    UpdateVertices(vertices, static_cast<int>(swapchainWidth), static_cast<int>(swapchainHeight));

    void* mapped = SDL_MapGPUTransferBuffer(m_device, m_uploadBuffer, true);
    if (mapped == nullptr) {
        LogSdlError("SDL_MapGPUTransferBuffer");
        SDL_CancelGPUCommandBuffer(commandBuffer);
        return;
    }

    std::memcpy(mapped, vertices.data(), vertices.size() * sizeof(Vertex));
    SDL_UnmapGPUTransferBuffer(m_device, m_uploadBuffer);

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    SDL_GPUTransferBufferLocation source = {};
    source.transfer_buffer = m_uploadBuffer;
    source.offset = 0;

    SDL_GPUBufferRegion destination = {};
    destination.buffer = m_vertexBuffer;
    destination.offset = 0;
    destination.size = static_cast<Uint32>(vertices.size() * sizeof(Vertex));

    SDL_UploadToGPUBuffer(copyPass, &source, &destination, true);
    SDL_EndGPUCopyPass(copyPass);

    SDL_GPUColorTargetInfo colorTarget = {};
    colorTarget.texture = swapchainTexture;
    colorTarget.mip_level = 0;
    colorTarget.layer_or_depth_plane = 0;
    colorTarget.clear_color = SDL_FColor{ 0.08f, 0.09f, 0.12f, 1.0f };
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;
    colorTarget.resolve_texture = nullptr;
    colorTarget.resolve_mip_level = 0;
    colorTarget.resolve_layer = 0;
    colorTarget.cycle = false;
    colorTarget.cycle_resolve_texture = false;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(renderPass, m_pipeline);

    SDL_GPUViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.w = static_cast<float>(swapchainWidth);
    viewport.h = static_cast<float>(swapchainHeight);
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    SDL_SetGPUViewport(renderPass, &viewport);

    SDL_GPUBufferBinding binding = {};
    binding.buffer = m_vertexBuffer;
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(renderPass, 0, &binding, 1);

    SDL_DrawGPUPrimitives(renderPass, static_cast<Uint32>(vertices.size()), 1, 0, 0);
    SDL_EndGPURenderPass(renderPass);

    if (!SDL_SubmitGPUCommandBuffer(commandBuffer)) {
        LogSdlError("SDL_SubmitGPUCommandBuffer");
    }
}

void App::LogSdlError(const char* context) {
    std::cerr << context << " failed: " << SDL_GetError() << std::endl;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    App app;
    if (!app.Initialize()) {
        app.Shutdown();
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
