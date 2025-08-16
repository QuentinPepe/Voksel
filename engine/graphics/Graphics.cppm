export module Graphics;

import Core.Types;
import Graphics.Window;
import std;

export struct Vertex {
    float position[3];
    float normal[3];
    float uv[2];
    U32 material;
};

export enum class PrimitiveTopology {
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList
};

export enum class ShaderStage {
    Vertex,
    Pixel,
    Geometry,
    Hull,
    Domain,
    Compute
};

export struct ShaderCode {
    std::string source;
    std::string entryPoint;
    ShaderStage stage;
};

export struct GraphicsPipelineCreateInfo {
    Vector<ShaderCode> shaders;
    Vector<std::pair<std::string, U32>> vertexAttributes;
    U32 vertexStride;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    bool depthTest = true;
    bool depthWrite = true;
};

export struct RenderPassInfo {
    std::string name;
    bool clearColor = true;
    bool clearDepth = true;
    float clearColorValue[4] = {0.0f, 0.2f, 0.4f, 1.0f};
    float clearDepthValue = 1.0f;
};

export class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;
    virtual U32 CreateVertexBuffer(const void* data, U64 size) = 0;
    virtual U32 CreateIndexBuffer(const void* data, U64 size) = 0;
    virtual void UpdateVertexBuffer(U32 buffer, const void* data, U64 size, U64 dstOffset = 0) = 0;
    virtual void UpdateIndexBuffer(U32 buffer, const void* data, U64 size, U64 dstOffset = 0) = 0;
    virtual U32 CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info) = 0;
    virtual U32 CreateConstantBuffer(U64 size) = 0;
    virtual void UpdateConstantBuffer(U32 buffer, const void* data, U64 size) = 0;
    virtual U32 CreateTexture2D(const void* rgba8, U32 width, U32 height, U32 mipLevels = 1) = 0;
    virtual void SetTexture(U32 texture, U32 slot) = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void BeginRenderPass(const RenderPassInfo& info) = 0;
    virtual void EndRenderPass() = 0;
    virtual void SetPipeline(U32 pipeline) = 0;
    virtual void SetVertexBuffer(U32 buffer) = 0;
    virtual void SetIndexBuffer(U32 buffer) = 0;
    virtual void SetConstantBuffer(U32 buffer, U32 slot) = 0;
    virtual void Draw(U32 vertexCount, U32 instanceCount = 1, U32 firstVertex = 0, U32 firstInstance = 0) = 0;
    virtual void DrawIndexed(U32 indexCount, U32 instanceCount = 1, U32 firstIndex = 0, S32 vertexOffset = 0, U32 firstInstance = 0) = 0;
    virtual void OnResize(U32 width, U32 height) = 0;
    [[nodiscard]] virtual bool ShouldClose() const = 0;
};

export struct GraphicsConfig {
    bool enableValidation = true;
    bool enableVSync = true;
    U32 frameBufferCount = 3;
};

export std::unique_ptr<IGraphicsContext> CreateDX12GraphicsContext(Window& window, const GraphicsConfig& config);

export inline std::unique_ptr<IGraphicsContext> CreateGraphicsContext(Window& window, const GraphicsConfig& config) {
    return CreateDX12GraphicsContext(window, config);
}