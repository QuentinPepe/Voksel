module;

#ifdef VOXEL_USE_DX11
#include <d3d11.h>
#include <wrl/client.h>
#endif

export module Graphics.Buffer;

import Core.Types;
import Core.Assert;
import Graphics.Shader;
import std;

#ifdef VOXEL_USE_DX11
using Microsoft::WRL::ComPtr;
#endif

export enum class BufferType : U8 {
    Vertex,
    Index,
    Constant,
    Structured
};

export enum class BufferUsage : U8 {
    Static, // GPU read-only
    Dynamic, // CPU write, GPU read
    Staging, // CPU read/write
};

export struct BufferDesc {
    BufferType type;
    BufferUsage usage;
    U32 size;
    U32 stride = 0;
    const void* initialData = nullptr;
};

#ifdef VOXEL_USE_DX11

export class Buffer {
private:
    ComPtr<ID3D11Buffer> m_Buffer;
    BufferDesc m_Desc;

public:
    Buffer(ID3D11Device* device, const BufferDesc& desc) : m_Desc{desc} {
        D3D11_BUFFER_DESC bufferDesc{};
        bufferDesc.ByteWidth = desc.size;
        bufferDesc.StructureByteStride = desc.stride;

        // Set usage
        switch (desc.usage) {
            case BufferUsage::Static:
                bufferDesc.Usage = D3D11_USAGE_DEFAULT;
                bufferDesc.CPUAccessFlags = 0;
                break;
            case BufferUsage::Dynamic:
                bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
                bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                break;
            case BufferUsage::Staging:
                bufferDesc.Usage = D3D11_USAGE_STAGING;
                bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
                break;
        }

        // Set bind flags
        switch (desc.type) {
            case BufferType::Vertex:
                bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                break;
            case BufferType::Index:
                bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                break;
            case BufferType::Constant:
                bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                break;
            case BufferType::Structured:
                bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
                break;
        }

        D3D11_SUBRESOURCE_DATA initData{};
        D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
        if (desc.initialData) {
            initData.pSysMem = desc.initialData;
            pInitData = &initData;
        }

        HRESULT hr = device->CreateBuffer(&bufferDesc, pInitData, &m_Buffer);
        assert(SUCCEEDED(hr), "Failed to create buffer");
    }

    void Update(ID3D11DeviceContext* context, const void* data, U32 size) {
        assert(m_Desc.usage == BufferUsage::Dynamic, "Can only update dynamic buffers");
        assert(size <= m_Desc.size, "Update size exceeds buffer size");

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = context->Map(m_Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        assert(SUCCEEDED(hr), "Failed to map buffer");

        std::memcpy(mappedResource.pData, data, size);
        context->Unmap(m_Buffer.Get(), 0);
    }

    [[nodiscard]] ID3D11Buffer* GetBuffer() const { return m_Buffer.Get(); }
    [[nodiscard]] const BufferDesc& GetDesc() const { return m_Desc; }
};

export class VertexBuffer : public Buffer {
public:
    VertexBuffer(ID3D11Device* device, const void* vertices, U32 vertexSize, U32 vertexCount, BufferUsage usage = BufferUsage::Static)
        : Buffer(device, BufferDesc{
            .type = BufferType::Vertex,
            .usage = usage,
            .size = vertexSize * vertexCount,
            .stride = vertexSize,
            .initialData = vertices
        }) {}

    void Bind(ID3D11DeviceContext* context, U32 slot = 0) const {
        ID3D11Buffer* buffer = GetBuffer();
        UINT stride = GetDesc().stride;
        UINT offset = 0;
        context->IASetVertexBuffers(slot, 1, &buffer, &stride, &offset);
    }
};

export class IndexBuffer : public Buffer {
private:
    DXGI_FORMAT m_Format;
    U32 m_IndexCount;
public:
    IndexBuffer(ID3D11Device* device, const void* indices, U32 indexSize, U32 indexCount, BufferUsage usage = BufferUsage::Static)
        : Buffer(device, BufferDesc{
            .type = BufferType::Index,
            .usage = usage,
            .size = indexSize * indexCount,
            .stride = indexSize,
            .initialData = indices
        }),
        m_Format{indexSize == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT},
        m_IndexCount{indexCount} {}

    void Bind(ID3D11DeviceContext* context) const {
        context->IASetIndexBuffer(GetBuffer(), m_Format, 0);
    }

    [[nodiscard]] U32 GetIndexCount() const { return m_IndexCount; }
};

export class ConstantBuffer : public Buffer {
public:
    ConstantBuffer(ID3D11Device* device, U32 size, const void* initialData = nullptr)
        : Buffer(device, BufferDesc{
            .type = BufferType::Constant,
            .usage = BufferUsage::Dynamic,
            .size = (size + 15) & ~15, // Align to 16 bytes
            .stride = 0,
            .initialData = initialData
        }) {}

    void Bind(ID3D11DeviceContext* context, ShaderType shaderType, U32 slot) const {
        ID3D11Buffer* buffer = GetBuffer();

        switch (shaderType) {
            case ShaderType::Vertex:
                context->VSSetConstantBuffers(slot, 1, &buffer);
                break;
            case ShaderType::Pixel:
                context->PSSetConstantBuffers(slot, 1, &buffer);
                break;
            case ShaderType::Geometry:
                context->GSSetConstantBuffers(slot, 1, &buffer);
                break;
            case ShaderType::Compute:
                context->CSSetConstantBuffers(slot, 1, &buffer);
                break;
        }
    }

};

#endif