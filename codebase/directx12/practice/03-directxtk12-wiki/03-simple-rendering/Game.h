//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"


// A basic game implementation that creates a D3D12 device and
// provides a game loop.
class Game final : public DX::IDeviceNotify
{
public:

    Game() noexcept(false);
    ~Game() = default;

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    // Initialization and management
    void Initialize(::IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnDisplayChange();
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
    void ValidateDevice();

    // Properties
    void GetDefaultSize( int& width, int& height ) const noexcept;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                               m_timer;

    // If using the DirectX Tool Kit for DX12, uncomment this line:
    std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory;

    using VertexType = DirectX::VertexPositionNormalTexture;

    std::unique_ptr<DirectX::NormalMapEffect> m_effect;
    std::unique_ptr<DirectX::PrimitiveBatch<VertexType>> m_batch;

    std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;

    enum Descriptors
    {
        Rocks,
        NormalMap,
        Count
    };

    std::unique_ptr<DirectX::CommonStates> m_states;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_normalMap;
};
