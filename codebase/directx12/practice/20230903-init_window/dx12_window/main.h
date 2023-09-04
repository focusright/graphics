#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

#include "d3dx12.h"
#include <dxgi1_6.h>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

static const UINT FrameCount = 2;

ComPtr<IDXGISwapChain3> m_swapChain;
ComPtr<ID3D12Device> m_device;
ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
ComPtr<ID3D12CommandAllocator> m_commandAllocator;
ComPtr<ID3D12CommandQueue> m_commandQueue;
ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
ComPtr<ID3D12PipelineState> m_pipelineState;
ComPtr<ID3D12GraphicsCommandList> m_commandList;
UINT m_rtvDescriptorSize;

UINT m_frameIndex;
HANDLE m_fenceEvent;
ComPtr<ID3D12Fence> m_fence;
UINT64 m_fenceValue;

UINT m_width = 1280;
UINT m_height = 720;
float m_aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);;
bool m_useWarpDevice;
static HWND m_hwnd;

inline std::string HrToString(HRESULT hr) {
    char s_str[64] = {};
    sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
    return std::string(s_str);
}

class HrException : public std::runtime_error {
    public:
        HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
        HRESULT Error() const { return m_hr; }
    private:
        const HRESULT m_hr;
};