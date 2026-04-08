#pragma once

#include <d3d12.h>

namespace D3DX12
{
    struct CD3DX12_HEAP_PROPERTIES : public D3D12_HEAP_PROPERTIES
    {
        explicit CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE type, UINT creationNodeMask = 1, UINT visibleNodeMask = 1) noexcept
        {
            Type = type;
            CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            CreationNodeMask = creationNodeMask;
            VisibleNodeMask = visibleNodeMask;
        }
    };

    struct CD3DX12_RESOURCE_DESC : public D3D12_RESOURCE_DESC
    {
        static inline CD3DX12_RESOURCE_DESC Buffer(UINT64 width, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, UINT64 alignment = 0) noexcept
        {
            CD3DX12_RESOURCE_DESC result = {};
            result.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            result.Alignment = alignment;
            result.Width = width;
            result.Height = 1;
            result.DepthOrArraySize = 1;
            result.MipLevels = 1;
            result.Format = DXGI_FORMAT_UNKNOWN;
            result.SampleDesc.Count = 1;
            result.SampleDesc.Quality = 0;
            result.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            result.Flags = flags;
            return result;
        }
    };

}
