#pragma once

#include <d3d12.h>
#include <DirectXMath.h>

#define D3DX12_DEFAULT 0
#define CD3DX12_DEFAULT D3DX12_DEFAULT

namespace D3DX12
{
    // CD3DX12_RANGE
    struct CD3DX12_RANGE : public D3D12_RANGE
    {
        CD3DX12_RANGE() = default;
        explicit CD3DX12_RANGE(const D3D12_RANGE& o) noexcept : D3D12_RANGE(o) {}
        CD3DX12_RANGE(SIZE_T begin, SIZE_T end) noexcept
        {
            Begin = begin;
            End = end;
        }
    };

    // CD3DX12_HEAP_PROPERTIES
    struct CD3DX12_HEAP_PROPERTIES : public D3D12_HEAP_PROPERTIES
    {
        CD3DX12_HEAP_PROPERTIES() = default;
        explicit CD3DX12_HEAP_PROPERTIES(const D3D12_HEAP_PROPERTIES& o) noexcept : D3D12_HEAP_PROPERTIES(o) {}
        CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY cpuPageProperty, D3D12_MEMORY_POOL memoryPoolPreference, UINT creationNodeMask = 1, UINT nodeMask = 1) noexcept
        {
            Type = D3D12_HEAP_TYPE_CUSTOM;
            CPUPageProperty = cpuPageProperty;
            MemoryPoolPreference = memoryPoolPreference;
            CreationNodeMask = creationNodeMask;
            VisibleNodeMask = nodeMask;
        }
        explicit CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE type, UINT creationNodeMask = 1, UINT nodeMask = 1) noexcept
        {
            Type = type;
            CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            CreationNodeMask = creationNodeMask;
            VisibleNodeMask = nodeMask;
        }
        bool IsCPUAccessible() const noexcept
        {
            return Type == D3D12_HEAP_TYPE_UPLOAD || Type == D3D12_HEAP_TYPE_READBACK || (Type == D3D12_HEAP_TYPE_CUSTOM &&
                (CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE || CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_BACK));
        }
    };

    // CD3DX12_RESOURCE_DESC
    struct CD3DX12_RESOURCE_DESC : public D3D12_RESOURCE_DESC
    {
        CD3DX12_RESOURCE_DESC() = default;
        explicit CD3DX12_RESOURCE_DESC(const D3D12_RESOURCE_DESC& o) noexcept : D3D12_RESOURCE_DESC(o) {}
        CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION dimension, UINT64 alignment, UINT64 width, UINT height, UINT16 depthOrArraySize, UINT16 mipLevels, DXGI_FORMAT format, UINT sampleCount, UINT sampleQuality, D3D12_TEXTURE_LAYOUT layout, D3D12_RESOURCE_FLAGS flags) noexcept
        {
            Dimension = dimension;
            Alignment = alignment;
            Width = width;
            Height = height;
            DepthOrArraySize = depthOrArraySize;
            MipLevels = mipLevels;
            Format = format;
            SampleDesc.Count = sampleCount;
            SampleDesc.Quality = sampleQuality;
            Layout = layout;
            Flags = flags;
        }
        static inline CD3DX12_RESOURCE_DESC Buffer(const D3D12_RESOURCE_ALLOCATION_INFO& resAllocInfo, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) noexcept
        {
            return CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER, resAllocInfo.Alignment, resAllocInfo.SizeInBytes, 1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags);
        }
        static inline CD3DX12_RESOURCE_DESC Buffer(UINT64 width, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, UINT64 alignment = 0) noexcept
        {
            return CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER, alignment, width, 1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags);
        }
    };

    // CD3DX12_CPU_DESCRIPTOR_HANDLE
    struct CD3DX12_CPU_DESCRIPTOR_HANDLE : public D3D12_CPU_DESCRIPTOR_HANDLE
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE() = default;
        explicit CD3DX12_CPU_DESCRIPTOR_HANDLE(const D3D12_CPU_DESCRIPTOR_HANDLE& o) noexcept : D3D12_CPU_DESCRIPTOR_HANDLE(o) {}
        CD3DX12_CPU_DESCRIPTOR_HANDLE(_In_ const D3D12_CPU_DESCRIPTOR_HANDLE& other, INT offsetScaledByIncrementSize) noexcept
        {
            InitOffsetted(other, offsetScaledByIncrementSize);
        }
        CD3DX12_CPU_DESCRIPTOR_HANDLE& Offset(INT offsetInDescriptors, UINT descriptorIncrementSize) noexcept
        {
            ptr += offsetInDescriptors * descriptorIncrementSize;
            return *this;
        }
        CD3DX12_CPU_DESCRIPTOR_HANDLE& Offset(UINT descriptorIncrementSize) noexcept
        {
            ptr += descriptorIncrementSize;
            return *this;
        }
        void InitOffsetted(_In_ const D3D12_CPU_DESCRIPTOR_HANDLE& base, INT offsetInDescriptors, UINT descriptorIncrementSize) noexcept
        {
            InitOffsetted(base, offsetInDescriptors * descriptorIncrementSize);
        }
        void InitOffsetted(_In_ const D3D12_CPU_DESCRIPTOR_HANDLE& base, INT offsetScaledByIncrementSize) noexcept
        {
            ptr = base.ptr + offsetScaledByIncrementSize;
        }
    };

    // CD3DX12_RESOURCE_BARRIER
    struct CD3DX12_RESOURCE_BARRIER : public D3D12_RESOURCE_BARRIER
    {
        CD3DX12_RESOURCE_BARRIER() = default;
        explicit CD3DX12_RESOURCE_BARRIER(const D3D12_RESOURCE_BARRIER& o) noexcept : D3D12_RESOURCE_BARRIER(o) {}
        static inline CD3DX12_RESOURCE_BARRIER Transition(_In_ ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter, UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE) noexcept
        {
            CD3DX12_RESOURCE_BARRIER result = {};
            D3D12_RESOURCE_BARRIER& b = result;
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Flags = flags;
            b.Transition.pResource = pResource;
            b.Transition.StateBefore = stateBefore;
            b.Transition.StateAfter = stateAfter;
            b.Transition.Subresource = subresource;
            return result;
        }
    };

    // CD3DX12_ROOT_SIGNATURE_DESC
    struct CD3DX12_ROOT_SIGNATURE_DESC : public D3D12_ROOT_SIGNATURE_DESC
    {
        CD3DX12_ROOT_SIGNATURE_DESC() = default;
        explicit CD3DX12_ROOT_SIGNATURE_DESC(const D3D12_ROOT_SIGNATURE_DESC& o) noexcept : D3D12_ROOT_SIGNATURE_DESC(o) {}
        CD3DX12_ROOT_SIGNATURE_DESC(UINT numParameters, _In_reads_opt_(numParameters) const D3D12_ROOT_PARAMETER* _pParameters, UINT numStaticSamplers = 0, _In_reads_opt_(numStaticSamplers) const D3D12_STATIC_SAMPLER_DESC* _pStaticSamplers = nullptr, D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
        {
            Init(numParameters, _pParameters, numStaticSamplers, _pStaticSamplers, flags);
        }
        CD3DX12_ROOT_SIGNATURE_DESC(int) noexcept
        {
            Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
        }
        void Init(UINT numParameters, _In_reads_opt_(numParameters) const D3D12_ROOT_PARAMETER* _pParameters, UINT numStaticSamplers = 0, _In_reads_opt_(numStaticSamplers) const D3D12_STATIC_SAMPLER_DESC* _pStaticSamplers = nullptr, D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
        {
            NumParameters = numParameters;
            pParameters = _pParameters;
            NumStaticSamplers = numStaticSamplers;
            pStaticSamplers = _pStaticSamplers;
            Flags = flags;
        }
    };

    // CD3DX12_VERTEX_BUFFER_VIEW
    struct CD3DX12_VERTEX_BUFFER_VIEW : public D3D12_VERTEX_BUFFER_VIEW
    {
        CD3DX12_VERTEX_BUFFER_VIEW() = default;
        explicit CD3DX12_VERTEX_BUFFER_VIEW(const D3D12_VERTEX_BUFFER_VIEW& o) noexcept : D3D12_VERTEX_BUFFER_VIEW(o) {}
        CD3DX12_VERTEX_BUFFER_VIEW(_In_ ID3D12Resource* pBuffer, UINT size, UINT stride) noexcept
        {
            BufferLocation = pBuffer->GetGPUVirtualAddress();
            SizeInBytes = size;
            StrideInBytes = stride;
        }
    };
}
