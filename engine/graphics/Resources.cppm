module;
#include <d3d12.h>
#include <d3dx12.h>
#include <wrl/client.h>

export module Graphics.DX12.Resource;

import Core.Assert;
import Core.Types;
import Graphics.DX12.Device;
import std;

using Microsoft::WRL::ComPtr;

export enum class ResourceType : U8 {
    Buffer,
    Texture1D,
    Texture2D,
    Texture3D,
    TextureCube
};

export enum class ResourceUsage : U32 {
    None = 0,
    VertexBuffer = 1 << 0,
    IndexBuffer = 1 << 1,
    ConstantBuffer = 1 << 2,
    ShaderResource = 1 << 3,
    UnorderedAccess = 1 << 4,
    RenderTarget = 1 << 5,
    DepthStencil = 1 << 6,
    IndirectArgs = 1 << 7,
    CopySource = 1 << 8,
    CopyDest = 1 << 9
};

inline ResourceUsage operator|(ResourceUsage a, ResourceUsage b) {
    return static_cast<ResourceUsage>(static_cast<U32>(a) | static_cast<U32>(b));
}

inline ResourceUsage operator&(ResourceUsage a, ResourceUsage b) {
    return static_cast<ResourceUsage>(static_cast<U32>(a) & static_cast<U32>(b));
}

export struct BufferDesc {
    U64 size;
    ResourceUsage usage;
    bool cpuAccessible = false;
};

export struct TextureDesc {
    U32 width;
    U32 height = 1;
    U32 depth = 1;
    U32 mipLevels = 1;
    U32 arraySize = 1;
    DXGI_FORMAT format;
    ResourceUsage usage;
    D3D12_RESOURCE_DIMENSION dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
};

export class Resource {
protected:
    ComPtr<ID3D12Resource> m_Resource;
    D3D12_RESOURCE_STATES m_CurrentState;
    ResourceType m_Type;
    bool m_IsTracked = true;

public:
    Resource(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES initialState, ResourceType type)
        : m_Resource{std::move(resource)}, m_CurrentState{initialState}, m_Type{type} {}

    [[nodiscard]] ID3D12Resource* GetResource() const { return m_Resource.Get(); }
    [[nodiscard]] D3D12_RESOURCE_STATES GetCurrentState() const { return m_CurrentState; }
    [[nodiscard]] ResourceType GetType() const { return m_Type; }
    [[nodiscard]] bool IsTracked() const { return m_IsTracked; }

    void SetCurrentState(D3D12_RESOURCE_STATES state) { m_CurrentState = state; }
    void SetTracked(bool tracked) { m_IsTracked = tracked; }

    [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const {
        return m_Resource->GetGPUVirtualAddress();
    }
};

export class Buffer : public Resource {
private:
    BufferDesc m_Desc;
    void* m_MappedData = nullptr;

public:
    Buffer(Device& device, const BufferDesc& desc)
        : Resource{nullptr, D3D12_RESOURCE_STATE_COMMON, ResourceType::Buffer}, m_Desc{desc} {

        D3D12_HEAP_PROPERTIES heapProps = {};
        D3D12_RESOURCE_DESC resourceDesc = {};

        if (desc.cpuAccessible) {
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
            m_CurrentState = D3D12_RESOURCE_STATE_GENERIC_READ;
        } else {
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
            m_CurrentState = D3D12_RESOURCE_STATE_COMMON;
        }

        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = desc.size;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc = {1, 0};
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = GetResourceFlags(desc.usage);

        assert(SUCCEEDED(device.GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            m_CurrentState,
            nullptr,
            IID_PPV_ARGS(&m_Resource)
        )), "Failed to create buffer");

        if (desc.cpuAccessible) {
            m_Resource->Map(0, nullptr, &m_MappedData);
        }
    }

    ~Buffer() {
        if (m_MappedData) {
            m_Resource->Unmap(0, nullptr);
        }
    }

    void UpdateData(const void* data, U64 size, U64 offset = 0) {
        assert(m_MappedData, "Buffer is not CPU accessible");
        assert(offset + size <= m_Desc.size, "Update exceeds buffer size");
        std::memcpy(static_cast<U8*>(m_MappedData) + offset, data, size);
    }

    [[nodiscard]] const BufferDesc& GetDesc() const { return m_Desc; }
    [[nodiscard]] void* GetMappedData() const { return m_MappedData; }

private:
    static D3D12_RESOURCE_FLAGS GetResourceFlags(ResourceUsage usage) {
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        if (static_cast<U32>(usage & ResourceUsage::UnorderedAccess)) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
        return flags;
    }
};

export class Texture : public Resource {
private:
    TextureDesc m_Desc;

public:
    Texture(Device& device, const TextureDesc& desc)
        : Resource{nullptr, D3D12_RESOURCE_STATE_COMMON, GetResourceType(desc.dimension)}, m_Desc{desc} {

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = desc.dimension;
        resourceDesc.Width = desc.width;
        resourceDesc.Height = desc.height;
        resourceDesc.DepthOrArraySize = (desc.dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? desc.depth : desc.arraySize;
        resourceDesc.MipLevels = desc.mipLevels;
        resourceDesc.Format = desc.format;
        resourceDesc.SampleDesc = {1, 0};
        resourceDesc.Layout = desc.layout;
        resourceDesc.Flags = GetResourceFlags(desc.usage);

        D3D12_CLEAR_VALUE* clearValue = nullptr;
        D3D12_CLEAR_VALUE optimizedClear = {};

        if (static_cast<U32>(desc.usage & ResourceUsage::RenderTarget)) {
            m_CurrentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
            optimizedClear.Format = desc.format;
            optimizedClear.Color[0] = 0.0f;
            optimizedClear.Color[1] = 0.0f;
            optimizedClear.Color[2] = 0.0f;
            optimizedClear.Color[3] = 1.0f;
            clearValue = &optimizedClear;
        } else if (static_cast<U32>(desc.usage & ResourceUsage::DepthStencil)) {
            m_CurrentState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            optimizedClear.Format = desc.format;
            optimizedClear.DepthStencil.Depth = 1.0f;
            optimizedClear.DepthStencil.Stencil = 0;
            clearValue = &optimizedClear;
        }

        assert(SUCCEEDED(device.GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            m_CurrentState,
            clearValue,
            IID_PPV_ARGS(&m_Resource)
        )), "Failed to create texture");
    }

    [[nodiscard]] const TextureDesc& GetDesc() const { return m_Desc; }

private:
    static ResourceType GetResourceType(D3D12_RESOURCE_DIMENSION dimension) {
        switch (dimension) {
            case D3D12_RESOURCE_DIMENSION_TEXTURE1D: return ResourceType::Texture1D;
            case D3D12_RESOURCE_DIMENSION_TEXTURE2D: return ResourceType::Texture2D;
            case D3D12_RESOURCE_DIMENSION_TEXTURE3D: return ResourceType::Texture3D;
            default: assert(false, "Invalid texture dimension"); return ResourceType::Texture2D;
        }
    }

    static D3D12_RESOURCE_FLAGS GetResourceFlags(ResourceUsage usage) {
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        if (static_cast<U32>(usage & ResourceUsage::RenderTarget)) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        if (static_cast<U32>(usage & ResourceUsage::DepthStencil)) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }
        if (static_cast<U32>(usage & ResourceUsage::UnorderedAccess)) {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
        if (!(static_cast<U32>(usage & ResourceUsage::ShaderResource))) {
            flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        }
        return flags;
    }
};