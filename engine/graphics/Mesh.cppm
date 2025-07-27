module;

#ifdef VOXEL_USE_DX11
#include <d3d11.h>
#endif

export module Graphics.Mesh;

import Graphics.Buffer;
import Core.Types;
import std;

export struct Vertex {
    F32 position[3];
    F32 color[4];

    static constexpr U32 GetStride() { return sizeof(Vertex); }
};

export class Mesh {
private:
    UniquePtr<VertexBuffer> m_VertexBuffer;
    UniquePtr<IndexBuffer> m_IndexBuffer;
    U32 m_VertexCount;
    U32 m_IndexCount;

public:
    Mesh() : m_VertexCount{0}, m_IndexCount{0} {}

    void SetVertexBuffer(UniquePtr<VertexBuffer> buffer, U32 count) {
        m_VertexBuffer = std::move(buffer);
        m_VertexCount = count;
    }

    void SetIndexBuffer(UniquePtr<IndexBuffer> buffer, U32 count) {
        m_IndexBuffer = std::move(buffer);
        m_IndexCount = count;
    }

#ifdef VOXEL_USE_DX11
    void Bind(ID3D11DeviceContext* context) const {
        if (m_VertexBuffer) {
            m_VertexBuffer->Bind(context);
        }
        if (m_IndexBuffer) {
            m_IndexBuffer->Bind(context);
        }
    }
#else // TODO
#endif

    [[nodiscard]] U32 GetVertexCount() const { return m_VertexCount; }
    [[nodiscard]] U32 GetIndexCount() const { return m_IndexCount; }
    [[nodiscard]] bool HasIndices() const { return m_IndexBuffer != nullptr; }
};

#ifdef VOXEL_USE_DX11

export UniquePtr<Mesh> CreateTriangleMesh(ID3D11Device* device) {
    Vertex vertices[] = {
        { {0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}, // Top - Red
        { {0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}}, // Right - Green
        { {-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}, // Left - Blue
    };

    auto mesh = std::make_unique<Mesh>();

    auto vertexBuffer = std::make_unique<VertexBuffer>(
        device,
        vertices,
        sizeof(Vertex),
        3
    );

    mesh->SetVertexBuffer(std::move(vertexBuffer), 3);

    return mesh;
}
#endif
